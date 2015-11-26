// EPOS Scheduler Abstraction Declarations

#ifndef __scheduler_h
#define __scheduler_h

#include <utility/list.h>
#include <cpu.h>
#include <machine.h>

__BEGIN_SYS

// All scheduling criteria, or disciplines, must define operator int() with
// the semantics of returning the desired order of a given object within the
// scheduling list
namespace Scheduling_Criteria
{
    // Priority (static and dynamic)
    class Priority
    {
    public:
        enum {
            MAIN   = 0,
            HIGH   = 1,
            NORMAL = (unsigned(1) << (sizeof(int) * 8 - 1)) - 3,
            LOW    = (unsigned(1) << (sizeof(int) * 8 - 1)) - 2,
            IDLE   = (unsigned(1) << (sizeof(int) * 8 - 1)) - 1
        };

        static const bool timed = false;
        static const bool dynamic = false;
        static const bool preemptive = true;

    public:
        Priority(int p = NORMAL): _priority(p) {}

        operator const volatile int() const volatile { return _priority; }

        void update() {}

    protected:
        volatile int _priority;
    };

    // Round-Robin
    class RR: public Priority
    {
    public:
        enum {
            MAIN   = 0,
            NORMAL = 1,
            IDLE   = (unsigned(1) << (sizeof(int) * 8 - 1)) - 1
        };

        static const bool timed = true;
        static const bool dynamic = false;
        static const bool preemptive = true;

    public:
        RR(int p = NORMAL): Priority(p) {}
    };

    // First-Come, First-Served (FIFO)
    class FCFS: public Priority
    {
    public:
        enum {
            MAIN   = 0,
            NORMAL = 1,
            IDLE   = (unsigned(1) << (sizeof(int) * 8 - 1)) - 1
        };

        static const bool timed = false;
        static const bool dynamic = false;
        static const bool preemptive = false;

    public:
        FCFS(int p = NORMAL); // Defined at Alarm
    };
    
    // Global Round-Robin
    class GRR: public RR
    {
    public:
        static const unsigned int HEADS = Traits<Machine>::CPUS;

    public:
        GRR(int p = NORMAL): RR(p) {}

        static unsigned int current_head() { return Machine::cpu_id(); }
    };

    class CFS: public Priority
        {
        public:
            enum {
                MAIN   = 0,
                NORMAL = 1,
                IDLE   = (unsigned(1) << (sizeof(int) * 8 - 1)) - 1
            };
            static const unsigned int QUEUES = 4; // Informa o número da sublista na lista
            static const bool timed = false;
            static const bool dynamic = false;
            static const bool preemptive = false;
            int id;

        public:
            CFS(int p = NORMAL,int _id = Machine::cpu_id()):id(_id){}; // Defined at Alarm
            static unsigned int current_head(){ return Machine::cpu_id(); };

        };

}


// Scheduling_Queue
template<typename T, typename R = typename T::Criterion>
class Scheduling_Queue: public Scheduling_List<T> {};

template<typename T>
class Scheduling_Queue<T, Scheduling_Criteria::GRR>:
public Multihead_Scheduling_List<T> {};

// Scheduler
// Objects subject to scheduling by Scheduler must declare a type "Criterion"
// that will be used as the scheduling queue sorting criterion (viz, through
// operators <, >, and ==) and must also define a method "link" to export the
// list element pointing to the object being handled.
template<typename T>
class Scheduler: public Scheduling_Queue<T>
{
private:
    typedef Scheduling_Queue<T> Base;

public:
    typedef typename T::Criterion Criterion;
    typedef Scheduling_List<T, Criterion> Queue;
    typedef typename Queue::Element Element;

public:
    Scheduler() {}

    unsigned int schedulables() { return Base::size(); }

    T * volatile chosen() {
    	return const_cast<T * volatile>((Base::chosen()) ? Base::chosen()->object() : 0);
    }

    void insert(T * obj) {
        db<Scheduler>(TRC) << "Scheduler[chosen=" << chosen() << "]::insert(" << obj << ")" << endl;
        Base::insert(obj->link());
    }

    T * remove(T * obj) {
        db<Scheduler>(TRC) << "Scheduler[chosen=" << chosen() << "]::remove(" << obj << ")" << endl;
        return Base::remove(obj->link()) ? obj : 0;
    }

    void suspend(T * obj) {
        db<Scheduler>(TRC) << "Scheduler[chosen=" << chosen() << "]::suspend(" << obj << ")" << endl;
        Base::remove(obj->link());
    }

    void resume(T * obj) {
        db<Scheduler>(TRC) << "Scheduler[chosen=" << chosen() << "]::resume(" << obj << ")" << endl;
        Base::insert(obj->link());
    }

    T * choose() {
        db<Scheduler>(TRC) << "Scheduler[chosen=" << chosen() << "]::choose() => ";

        T * obj = Base::chosen() ? Base::choose()->object() : 0;

        db<Scheduler>(TRC) << obj << endl;

        return obj;
    }

    T * choose_another() {
        db<Scheduler>(TRC) << "Scheduler[chosen=" << chosen() << "]::choose_another() => ";

        T * obj = Base::choose_another()->object();

        db<Scheduler>(TRC) << obj << endl;

        return obj;
    }

    T * choose(T * obj) {
        db<Scheduler>(TRC) << "Scheduler[chosen=" << chosen() << "]::choose(" << obj;

        if(!Base::choose(obj->link()))
            obj = 0;

        db<Scheduler>(TRC) << obj << endl;

        return obj;
    }

};
// Scheduling_Queue_Multi_List
template<typename T,
		 typename R = typename T::Criterion>
class Scheduling_Queue_Multi_List: public Scheduling_Multilist<T, R> {};


// Scheduler_MultiList
// Using Multihead_Scheduling_List instead of Scheduling_List
template<typename T,
		 typename R = typename T::Criterion>
class Scheduler_MultiList: public Scheduling_Queue_Multi_List<T, R>
{
private:
    typedef Scheduling_Queue_Multi_List<T> Base;

public:
    typedef typename T::Criterion Criterion;
    typedef Multihead_Scheduling_List<T, Criterion> Queue;
    typedef typename Queue::Element Element;

public:
    Scheduler_MultiList() {}

    unsigned int schedulables() { return Base::size(); }

    T * volatile chosen() {
    	// If called before insert(), chosen will dereference a null pointer!
    	// For threads, we this won't happen (see Thread::init()).
    	// But if you are unsure about your new use of the scheduler,
    	// please, pay the price of the extra "if" bellow.
//    	return const_cast<T * volatile>((Base::chosen()) ? Base::chosen()->object() : 0);
    	return const_cast<T * volatile>(Base::chosen()->object());
    }

    void insert(T * obj) {
       db<Scheduler_MultiList>(TRC) << "Scheduler[chosen=" << chosen() << "]::insert(" << obj << ")" << endl;
        unsigned int list;
        if(obj->link()->rank() == Criterion::IDLE || obj->link()->rank() == Criterion::MAIN){
        	list = Machine:: cpu_id();
        }else
        	list = choose_list();


        obj->link()->cpu_id = list;
        Base::insert(obj->link());
        if(list != Machine::cpu_id())
        	APIC::ipi_send(list,49);
        //envia interrupcao
    }

    int choose_list(){
    	int ncpus = Machine::n_cpus();
    	unsigned int menor = 100;
    	int lista;
    	for(int i= 0; i<ncpus;i++){
    		if(Base::_list[i].size()<menor){
    			lista = i;
    			menor = Base::_list[i].size();
    		}
    	}

    	return lista;

    }

    T * remove(T * obj) {
        db<Scheduler_MultiList>(TRC) << "Scheduler[chosen=" << chosen() << "]::remove(" << obj << ")" << endl;
        unsigned int list = obj->link()->cpu_id;
        T * o = Base::remove(obj->link()) ? obj : 0;
        if( list!= Machine::cpu_id())
			APIC::ipi_send(list,49);
                //envia interrupcao
        return o;
    }

    void suspend(T * obj) {
        db<Scheduler_MultiList>(TRC) << "Scheduler[chosen=" << chosen() << "]::suspend(" << obj << ")" << endl;
        unsigned int list = obj->link()->cpu_id;
        Base::remove(obj->link());
        if( list!= Machine::cpu_id())
        	APIC::ipi_send(list,49);
                        //envia interrupcao
    }

    void resume(T * obj) {
        db<Scheduler_MultiList>(TRC) << "Scheduler[chosen=" << chosen() << "]::resume(" << obj << ")" << endl;
        unsigned int list = obj->link()->cpu_id;
        Base::insert(obj->link());
        if( list!= Machine::cpu_id())
        	IC::ipi_send(list,49);
             //envia interrupcao
    }

    T * choose() {
        db<Scheduler_MultiList>(TRC) << "Scheduler[chosen=" << chosen() << "]::choose() => ";

        T * obj = Base::choose()->object();

        db<Scheduler_MultiList>(TRC) << obj << endl;

        return obj;
    }

    T * choose_another() {
        db<Scheduler_MultiList>(TRC) << "Scheduler[chosen=" << chosen() << "]::choose_another() => ";

        T * obj = Base::choose_another()->object();

        db<Scheduler_MultiList>(TRC) << obj << endl;

        return obj;
    }

    T * choose(T * obj) {
        db<Scheduler_MultiList>(TRC) << "Scheduler[chosen=" << chosen() << "]::choose(" << obj;

        if(!Base::choose(obj->link()))
            obj = 0;

        db<Scheduler_MultiList>(TRC) << obj << endl;

        return obj;
    }
};

__END_SYS

#endif