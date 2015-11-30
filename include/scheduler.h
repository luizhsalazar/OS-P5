// EPOS Scheduler Abstraction Declarations

#ifndef __scheduler_h
#define __scheduler_h

#include <utility/list.h>
#include <cpu.h>
#include <machine.h>
#include <utility/random.h>
#include <utility/ostream.h>
#include <display.h>


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

    template<typename T>
    class CFS: public Priority
        {
        public:
            enum {
                MAIN   = 0,
                NORMAL = 1,
                IDLE   = (unsigned(1) << (sizeof(int) * 8 - 1)) - 1
            };
            static const unsigned int QUEUES = 4; // Informa o número da sublista na lista

		    static const bool timed = true;
            static const bool dynamic = false;
            static const bool preemptive = true;

        public:
            // DEFINE A CPU QUE A THREAD TERÁ AFINIDADE (Baseado no número de threads em cada lista)
            CFS(int p = NORMAL): Priority(p) {}

            static unsigned int current_queue(){ return Machine::cpu_id(); };
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
template<typename T>
class Scheduler_MultiList: public Scheduling_Queue_Multi_List<T, Scheduling_Criteria::CFS<T>>
{
private:
    typedef Scheduling_Queue_Multi_List<T> Base;

public:
    typedef typename T::Criterion Criterion;
    typedef Scheduling_Multilist<T, Criterion> Queue;
    typedef typename Queue::Element Element;
    OStream cout;

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

    void migration_needed() {

    	int ncpus = Machine::n_cpus();
		double media_cpu[ncpus];
		double menor= 100000000;
		unsigned int id_menor_cpu = 0;
		double maior= 0;
		unsigned int id_maior_cpu = 0;

		for(int i=0; i < ncpus; i++){

			int size_list = (int) Base::_list[i].size();
			int soma_cpu = 0;

			Element * e = Base::_list[i].head();
			if (e == 0) {
				return;
			}

			T * t = e->object();
			soma_cpu += t->get_soma_percentage();

			/* size - 2 para a next() nunca ser a IDLE */
			for(int j=0; j < size_list-2; j++){
				e = e->next();
				t = e->object();
				soma_cpu += t->get_soma_percentage();
			}

			if (size_list != 0){
				media_cpu[i] = soma_cpu/size_list;
			}

			if(media_cpu[i] < menor){
				menor = media_cpu[i];
				id_menor_cpu = i;
			}

			if(media_cpu[i] > maior){
				maior = media_cpu[i];
				id_maior_cpu = i;
			}
		}

		if(Machine::cpu_id() == id_maior_cpu && maior != menor){
			int qtd = ((Base::_list[id_maior_cpu].size() - Base::_list[id_menor_cpu].size()) /2) + 1;
			migrate(id_menor_cpu, qtd);
		}
	}

	void migrate(int new_affinity, int qtd_threads) {

		for(int i = 0; i < qtd_threads; i++){

			T * thread = Base::_list[Machine::cpu_id()].head()->object();

			if (thread->link()->rank() == Criterion::IDLE){
				return;
			}

			remove(thread);
			thread->_affinity = new_affinity;
			insert(thread);

			APIC::ipi_send(new_affinity, IC::INT_RESCHEDULER);
		}

	}

	int calculate_fairness(){
		int ncpus = Machine::n_cpus();
		double menor= 100000000;
		double maior= 0;

		for(int i=0; i<ncpus; i++){
			int soma_thread=0;
			Element * e = Base::_list[i].head();
			T * t = e->object();
			soma_thread += t->get_soma_percentage();

			for(unsigned int j=0;j<Base::_list[i].size()-1;j++){
				e = e->next();
				t = e->object();
				soma_thread += t->get_soma_percentage();
				if(soma_thread < menor){
				   menor = soma_thread;
				} else if(soma_thread>maior){
					maior = soma_thread;
				}
			}
		}
		return maior - menor;
   }

    void insert(T * obj) {
       db<Scheduler_MultiList>(TRC) << "Scheduler[chosen=" << chosen() << "]::insert(" << obj << ")" << endl;

       Base::insert(obj->link());
    }

    T * remove(T * obj) {
        db<Scheduler_MultiList>(TRC) << "Scheduler[chosen=" << chosen() << "]::remove(" << obj << ")" << endl;

        return Base::remove(obj->link()) ? obj : 0;
    }

    void suspend(T * obj) {
        db<Scheduler_MultiList>(TRC) << "Scheduler[chosen=" << chosen() << "]::suspend(" << obj << ")" << endl;

        Base::remove(obj->link());
    }

    void resume(T * obj) {
        db<Scheduler_MultiList>(TRC) << "Scheduler[chosen=" << chosen() << "]::resume(" << obj << ")" << endl;

        Base::insert(obj->link());
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
