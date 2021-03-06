// Honeycomb, Copyright (C) 2015 NewGamePlus Inc.  Distributed under the Boost Software License v1.0.
#pragma once

#include "Honey/Thread/Lock/Spin.h"

namespace honey { namespace lockfree
{

/// Deque that is lock-free only when used by a single producer and consumer, otherwise contention is split between front and back locks.
/**
  * Automatically expands storage size as needed (requires lock).
  *
  * Internally maintains a ring-buffer (traversing from head to tail may loop around end of buffer).
  */
template<class T, class Alloc_ = typename defaultAllocator<T>::type>
class SpscDeque : mt::NoCopy
{
public:
    typedef T value_type;
    typedef typename Alloc_::template rebind<T>::other Alloc;

    SpscDeque(szt capacity = 0, const Alloc& alloc = Alloc()) :
        _alloc(alloc),
        _capacity(0),
        _size(0),
        _head(0),
        _tail(0)
    {
        reserve(capacity);
    }

    ~SpscDeque()                                            { clear(); }

    /// Ensure that enough storage is allocated for a number of elements
    void reserve(szt capacity)
    {
        SpinLock::Scoped _(_headLock);
        SpinLock::Scoped __(_tailLock);
        if (_capacity < capacity) setCapacity(capacity);
    }

    /// The number of elements for which storage is allocated
    szt capacity() const                                    { return _capacity; }

    /// Resize the deque to contain a number of elements
    void resize(szt size, const T& initVal = T())
    {
        SpinLock::Scoped _(_headLock);
        SpinLock::Scoped __(_tailLock);
        setCapacity(size);
        //Init new data
        sdt dif = _capacity - _size;
        for (sdt i = 0; i < dif; ++i) _alloc.construct(_data + ringIndex(_head+_size+i), initVal);
        _size = size;
        _tail = _head;
    }

    /// Insert new element constructed with `val` at the beginning of the list
    template<class T_>
    void push_front(T_&& val)
    {
        //At size == 0, head and tail are vying to push the same first spot
        //At size == capacity-1, head and tail are vying to push the same last spot
        //At size == capacity, expansion is needed
        SpinLock::Scoped _(_headLock);
        SpinLock::Scoped __(_tailLock, _size == 0 || _size >= _capacity-1 ? lock::Op::lock : lock::Op::defer);
        if (_size == _capacity) expand();
        _head = ringDec(_head);
        _alloc.construct(_data + _head, forward<T_>(val));
        ++_size;
    }

    /// Add new element constructed with `val` onto the end of the list
    template<class T_>
    void push_back(T_&& val)
    {
        SpinLock::Scoped headLock(_headLock, lock::Op::defer);
        SpinLock::Scoped tailLock(_tailLock);
        //Lock head first to prevent deadlock
        if (_size == 0 || _size >= _capacity-1) { tailLock.unlock(); headLock.lock(); tailLock.lock(); }
        if (_size == _capacity) expand();
        _alloc.construct(_data + _tail, forward<T_>(val));
        _tail = ringInc(_tail);
        ++_size;
    }

    /// Pop element from the beginning of the list and move it into `val`.  Returns true on success, false if there is no element to pop.
    bool pop_front(optional<T&> val = optnull)
    {
        //At size == 1, head and tail are vying to pop the last spot
        SpinLock::Scoped _(_headLock);
        SpinLock::Scoped __(_tailLock, _size == 1 ? lock::Op::lock : lock::Op::defer);
        if (_size == 0) return false;
        if (val) val = move(_data[_head]);
        _alloc.destroy(_data + _head);
        _head = ringInc(_head);
        --_size;
        return true;
    }

    /// Pop element from the end of the list and move it into `val`.  Returns true on success, false if there is no element to pop.
    bool pop_back(optional<T&> val = optnull)
    {
        SpinLock::Scoped headLock(_headLock, lock::Op::defer);
        SpinLock::Scoped tailLock(_tailLock);
        if (_size == 1) { tailLock.unlock(); headLock.lock(); tailLock.lock(); }
        if (_size == 0) return false;
        _tail = ringDec(_tail);
        if (val) val = move(_data[_tail]);
        _alloc.destroy(_data + _tail);
        --_size;
        return true;
    }

    /// Remove all elements
    void clear()                                            { while (pop_back()); }

    /// Check whether the deque does not contain any elements
    bool empty() const                                      { return !_size; }
    /// Number of elements in deque
    szt size() const                                        { return _size; }

private:
    szt ringIndex(szt index) const                          { return index % _capacity; }
    szt ringInc(szt index) const                            { return index >= _capacity - 1 ? 0 : index + 1; }
    szt ringDec(szt index) const                            { return index == 0 ? _capacity - 1 : index - 1; }

    void setCapacity(szt capacity)
    {
        if (capacity == _capacity) return;
        //Get size (active element count) of new array, may be smaller than old
        szt size = capacity < _size ? capacity : _size.load();
        //Alloc new array
        T* data = nullptr;
        if (capacity > 0)
        {
            data = _alloc.allocate(capacity);
            //Copy active elements to new array (new head is at 0)
            if (_size > 0)
            {
                szt copyTail = ringIndex(_head + size);
                if (copyTail > _head)
                    //Contiguous region
                    std::copy(_data.get() + _head, _data + copyTail, data);
                else
                {
                    //Split region, loops around end
                    std::copy(_data.get() + _head, _data + _capacity, data);
                    std::copy(_data.get(), _data + copyTail, data + (_capacity - _head));
                }
            }
        }
        //Destroy any active elements that don't fit into new array
        sdt dif = _size - size;
        for (sdt i = 0; i < dif; ++i) _alloc.destroy(_data + ringIndex(_head+size+i));
        //Set new array
        _data.set(data);
        _data.finalizer() = finalize<T, Alloc>(_alloc, capacity);
        _capacity = capacity;
        _size = size;
        _head = 0;
        _tail = size;
    }

    void expand()                                           { setCapacity(_capacity + _capacity/2 + 1); } //Expand 50%

    Alloc           _alloc;
    UniquePtr<T, finalize<T,Alloc>> _data;
    szt             _capacity;
    Atomic<szt>     _size;
    szt             _head;
    szt             _tail;
    SpinLock        _headLock;
    SpinLock        _tailLock;
};

} }
