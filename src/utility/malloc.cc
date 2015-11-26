// EPOS Application-level Dynamic Memory Utility Implementation

#include <system/config.h>
#include <utility/malloc.h>

// C++ dynamic memory deallocators
void operator delete(void * object) {
    return free(object);
}

void operator delete[](void * object) {
    return free(object);
}

// Standard C Library allocators
extern "C"
{
    __USING_SYS;
    static bool _malloc_int_enabled;
    static Spin _malloc_lock;

    inline void _malloc_enter() {
        _malloc_int_enabled = CPU::int_enabled();
        CPU::int_disable();
        _malloc_lock.acquire();
    }

    inline void _malloc_leave() {
        _malloc_lock.release();
        if(_malloc_int_enabled)
            CPU::int_enable();
    }

    void * malloc(size_t bytes) {
        _malloc_enter();
        void * tmp = (Traits<System>::multiheap) ? Application::_heap->alloc(bytes) : System::_heap->alloc(bytes);
        _malloc_leave();
        return tmp;
    }

    void free(void * ptr) {
        _malloc_enter();
        if(Traits<System>::multiheap)
            Heap::typed_free(ptr);
        else
            Heap::untyped_free(System::_heap, ptr);
        _malloc_leave();
    }
}
