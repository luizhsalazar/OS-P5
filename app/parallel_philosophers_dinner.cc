// EPOS Scheduler Test Program

#include <utility/ostream.h>
#include <machine.h>
#include <display.h>
#include <thread.h>
#include <semaphore.h>
#include <alarm.h>
#include <string.h>

using namespace EPOS;

const int iterations = 10;

Semaphore table;

Thread * phil[5];
Semaphore * chopstick[5];

OStream cout;

int philosopher(int n, int l, int c);
void think(unsigned long long n);
void eat(unsigned long long n);
unsigned long long busy_wait(unsigned long long n);
unsigned long coutTotalTime[5];
unsigned long coutTotalTimeCore[5][4];

void printThreads(int i){
    table.p();
    Display::position(19, 0);
    cout << " ------------------------------------------------------------------- ";
    Display::position(20, 0);
	cout << " Thread "<<"   cpu0   cpu1  cpu2  cpu3    "<<" Tempo total ";
//    for(int i = 0; i < 5; i++) {
    	Display::position(21 + (i), 0);
    	cout << " Thread " << i;
		coutTotalTime[i] = coutTotalTime[i]+phil[i]->get_time();
		coutTotalTimeCore[i][Machine::cpu_id()] += phil[i]->get_time();
		Display::position(21 + (i),10+ Machine::cpu_id()*7);
		cout << "  " <<coutTotalTimeCore[i][Machine::cpu_id()];
    	Display::position(21 + (i), 10+ 5*7);
    	cout<<coutTotalTime[i];
//    }
    Display::position(27, 0);
    cout << " ------------------------------------------------------------------- ";
    table.v();
}



int main()
{
    table.p();
    Display::clear();
    Display::position(0, 0);
    cout << "The Philosopher's Dinner:" << endl;

    for(int i = 0; i < 5; i++){
        chopstick[i] = new Semaphore;
        coutTotalTime[i] = 0;
    }

    phil[0] = new Thread(&philosopher, 0,  5, 60);
    phil[1] = new Thread(&philosopher, 1, 10, 74);
    phil[2] = new Thread(&philosopher, 2, 16, 69);
    phil[3] = new Thread(&philosopher, 3, 16, 51);
    phil[4] = new Thread(&philosopher, 4, 10, 47);

    cout << "Philosophers are alive and hungry!" << endl;

    Display::position(7, 74);
    cout << '/';
    Display::position(13, 74);
    cout << '\\';
    Display::position(16, 65);
    cout << '|';
    Display::position(13, 57);
    cout << '/';
    Display::position(7, 57);
    cout << '\\';
    Display::position(18, 30);

    cout << "The dinner is served ..." << endl;
    table.v();

    for(int i = 0; i < 5; i++) {
        int ret = phil[i]->join();
        table.p();
        Display::position(30 + i, 0);
        cout << "Philosopher " << i << " ate " << ret << " times " << endl;
        table.v();

    }

    for(int i = 0; i < 5; i++)
        delete chopstick[i];
    for(int i = 0; i < 5; i++)
        delete phil[i];

    cout << "The end!" << endl;

    return 0;
}

int philosopher(int n, int l, int c)
{
    int first = (n < 4)? n : 0;
    int second = (n < 4)? n + 1 : 4;

    for(int i = iterations; i > 0; i--) {

    	printThreads(n);
        table.p();
        Display::position(l, c);
        cout << "thinking[" << Machine::cpu_id() << "]";
        table.v();

        think(10000);

        table.p();
        Display::position(l, c);
        cout << "  hungry[" << Machine::cpu_id() << "]";
        table.v();

        chopstick[first]->p();   // get first chopstick
        chopstick[second]->p();  // get second chopstick

        table.p();
        Display::position(l, c);
        cout << " eating[" << Machine::cpu_id() << "] ";
        table.v();

        eat(50000);

        table.p();
        Display::position(l, c);
        cout << "    sate[" << Machine::cpu_id() << "]";
        table.v();

        chopstick[first]->v();   // release first chopstick
        chopstick[second]->v();  // release second chopstick
    }

    table.p();
    Display::position(l, c);
    cout << "  done[" << Machine::cpu_id() << "]  ";
    table.v();


    return iterations;
}


void eat(unsigned long long n) {
    static unsigned long long v;
    v = busy_wait(n);
}

void think(unsigned long long n) {
    static unsigned long long v;
    v = busy_wait(n);
}

unsigned long long busy_wait(unsigned long long n)
{
    volatile unsigned long long v;
    for(long long int j = 0; j < 20 * n; j++)
        v &= 2 ^ j;
    return v;
}
