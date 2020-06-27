////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntimeWorldController.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniInternal.h"

#include "JaniRuntimeWorkerReference.h" // TODO: Move to .cpp

/////////////
// FORWARD //
/////////////
namespace nonstd
{
    template <class T>
    struct thread_local_frame_allocator;
}

///////////////
// NAMESPACE //
///////////////

///////////////////////////////
namespace nonstd { // NONSTD //
///////////////////////////////

    inline unsigned int pow2_roundup(unsigned int x)
    {
        if (x < 0)
            return 0;
        --x;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        return x + 1;
    }

    /*
     * This allocator will perform any subsequent required allocation in blocks of the given size (default is 2048), these
     * blocks cannot be directly deallocated, the entire allocator must be reseted in order to free the used data, thats why
     * its called frame block allocator
     * If data bigger than the block size is requested, a special allocation will be performed and a "big block" will be stored
     * in a separated list, that when freed can be reused, without any guarantees of fragmentation
     */
    template<uint32_t DefaultBlockSize = 2048>
    class frame_block_allocator
    {
    public:

        // The default block type
        struct DefaultBlock
        {
            uint8_t data[DefaultBlockSize];
            uint32_t amount      = DefaultBlockSize;
            uint32_t used_amount = 0;
        };

        // The large block type
        struct LargeBlock
        {
            LargeBlock(uint32_t _amount)
            {
                // Set the initial data
                data = new uint8_t[_amount];
                amount = _amount;
            }
            ~LargeBlock()
            {
                if (data != nullptr)
                {
                    delete[] data;
                }
            }

            uint8_t* data        = nullptr;
            uint32_t used_amount = 0;
            uint32_t amount      = -1;
        };

    //////////////////
    // CONSTRUCTORS //
    public: //////////

        // Constructor / destructor
        frame_block_allocator()
        {

        }
        ~frame_block_allocator()
        {

        }

    //////////////////
    // MAIN METHODS //
    public: //////////

        // Allocate
        template<typename ObjectType>
        ObjectType* allocate(const ObjectType* _data = nullptr)
        {
            // Get the size of this object
            uint32_t objectSize = nonstd::pow2_roundup(sizeof(ObjectType));

            // Check is the size of this object is higher then the default
            ObjectType* allocatedData = nullptr;
            if (objectSize >= DefaultBlockSize)
            {
                allocatedData = new (AllocateFromLargeBlocks(objectSize)) ObjectType;
            }
            else
            {
                allocatedData = new (AllocateFromDefaultBlocks(objectSize)) ObjectType;
            }

            // Check if we should copy the input data
            if (_data != nullptr)
            {
                std::memcpy(allocatedData, _data, sizeof(ObjectType));
            }

            return allocatedData;
        }

        // Allocate
        template<typename ObjectType>
        ObjectType* allocate(const ObjectType& _data)
        {
            // Get the size of this object
            uint32_t objectSize = nonstd::pow2_roundup(sizeof(ObjectType));

            // Check is the size of this object is higher then the default
            ObjectType* allocatedData = nullptr;
            if (objectSize >= DefaultBlockSize)
            {
                allocatedData = new (AllocateFromLargeBlocks(objectSize)) ObjectType(_data);
            }
            else
            {
                allocatedData = new (AllocateFromDefaultBlocks(objectSize)) ObjectType(_data);
            }

            return allocatedData;
        }

        // Allocate
        template<typename ObjectType>
        ObjectType* allocate(ObjectType&& _data)
        {
            // Get the size of this object
            uint32_t objectSize = nonstd::pow2_roundup(sizeof(ObjectType));

            // Check is the size of this object is higher then the default
            ObjectType* allocatedData = nullptr;
            if (objectSize >= DefaultBlockSize)
            {
                allocatedData = new (AllocateFromLargeBlocks(objectSize)) ObjectType(std::move(_data));
            }
            else
            {
                allocatedData = new (AllocateFromDefaultBlocks(objectSize)) ObjectType(std::move(_data));
            }

            return allocatedData;
        }

        // Allocate
        uint8_t* allocate(uint32_t _size, const uint8_t* _data = nullptr)
        {
            // Get the size of this object
            uint32_t objectSize = pow2_roundup(sizeof(uint8_t) * _size);

            // Check is the size of this object is higher then the default
            uint8_t* allocatedData = nullptr;
            if (objectSize >= DefaultBlockSize)
            {
                allocatedData = (uint8_t*)AllocateFromLargeBlocks(objectSize);
            }
            else
            {
                allocatedData = (uint8_t*)AllocateFromDefaultBlocks(objectSize);
            }

            // Check if we should copy the input data
            if (_data != nullptr)
            {
                std::memcpy(allocatedData, _data, sizeof(uint8_t) * _size);
            }

            return allocatedData;
        }

        // Reset
        void reset()
        {
            // For each default block on the list
            for (auto it = m_default_blocks.begin(); it != m_default_blocks.end(); ++it)
            {
                // Reset this block
                it->get()->used_amount = 0;
            }

            // For each large block on the list
            for (auto it = m_large_blocks.begin(); it != m_large_blocks.end(); ++it)
            {
                // Reset this block
                it->get()->used_amount = 0;
            }
        }

    private:

        // Allocate from the default blocks
        void* AllocateFromDefaultBlocks(uint32_t _dataSize)
        {
            // For each default block on the list
            for (auto it = m_default_blocks.begin(); it != m_default_blocks.end(); ++it)
            {
                // Check if we can allocate from here
                if (DetermineRemainingSpace(*it->get()) >= _dataSize)
                {
                    // Allocate from this block
                    return AllocateData(*it->get(), _dataSize);
                }
            }

            // Add a new block
            auto newBlock = std::make_unique<DefaultBlock>();
            m_default_blocks.insert(m_default_blocks.end(), std::move(newBlock));

            // Allocate from the new block
            return AllocateData(*m_default_blocks.back().get(), _dataSize);
        }

        // Allocate from the large blocks
        void* AllocateFromLargeBlocks(uint32_t _dataSize)
        {
            // For each large block on the list
            for (auto it = m_large_blocks.begin(); it != m_large_blocks.end(); ++it)
            {
                // Check if we can allocate from here
                if (DetermineRemainingSpace(*it->get()) >= _dataSize)
                {
                    // Allocate from this block
                    return AllocateData(*it->get(), _dataSize);
                }
            }

            // Add a new block
            auto newBlock = std::make_unique<LargeBlock>(_dataSize);
            m_large_blocks.insert(m_large_blocks.end(), std::move(newBlock));

            // Allocate from the new block
            return AllocateData(*m_large_blocks.back().get(), _dataSize);
        }

        // Calculate remaining free data 
        template<typename BlockType>
        uint32_t DetermineRemainingSpace(const BlockType& _block) const
        {
            return _block.amount - _block.used_amount;
        }

        // Allocate data from the given block
        template<typename BlockType>
        void* AllocateData(BlockType& _block, uint32_t _amount)
        {
            // Get the data and update the used amount
            void* data = &_block.data[_block.used_amount];
            _block.used_amount += _amount;

            return data;
        }

        ///////////////
        // VARIABLES //
    private: //////

        // The list of default blocks
        std::list<std::unique_ptr<DefaultBlock>> m_default_blocks;

        // The list of default blocks
        std::list<std::unique_ptr<LargeBlock>> m_large_blocks;
    };

//////////////
} // NONSTD //
//////////////

// Jani
JaniNamespaceBegin(Jani)

    /*
     * This class will be instantiated for each thread and will be used to keep information and allocators
     * located in one place
     */
    class ThreadLocalStorage
    {
        template <class T>
        friend struct nonstd::thread_local_frame_allocator;

    public:

        struct GlobalThreadStoragesWrapper
        {
            GlobalThreadStoragesWrapper()                                   = delete;
            GlobalThreadStoragesWrapper(const GlobalThreadStoragesWrapper&) = delete;
            GlobalThreadStoragesWrapper(const std::vector<ThreadLocalStorage*>& _storages, std::mutex& _storage_mutex) :
                storages(_storages),
                storage_mutex(_storage_mutex)
            {
                storage_mutex.get().lock();
            }

            ~GlobalThreadStoragesWrapper()
            {
                storage_mutex.get().unlock();
            }

            std::reference_wrapper < const std::vector<ThreadLocalStorage*>> storages;

        private:

            std::reference_wrapper<std::mutex> storage_mutex;
        };

    public:

        /*
         * Return the local storage for the callee thread
         */
        static ThreadLocalStorage& GetThreadLocalStorage();

        /*
         * Return a vector that contains all thread local storages
         */
        static GlobalThreadStoragesWrapper GetAllThreadLocalStorages();

        /*
        *
        */
        static void AcknowledgeFrameEnd();

        /*
        * Lock the global thread storage mutex, this should be used whenever 
        */

    public:

        ThreadLocalStorage();
        ~ThreadLocalStorage();

        /*
        * Return the current thread unique index
        */
        uint32_t GetUniqueIndex() const;



    protected:

        nonstd::frame_block_allocator<>& GetFrameAllocator() const;
        
    private:

        uint32_t m_unique_index = std::numeric_limits<uint32_t>::max();

        // Allocators
        std::unique_ptr<nonstd::frame_block_allocator<>> m_frame_allocator;
    };

/*
 * Ensure that the current code is being run on a certain thread type only
 */
#define EnsureGameWorkerThread()   assert(ThreadLocalStorage::GetThreadLocalStorage().GetType() == ThreadType::GameWorkerThread)
#define EnsureRenderWorkerThread() assert(ThreadLocalStorage::GetThreadLocalStorage().GetType() == ThreadType::RenderWorkerThread)
#define EnsureWorkerThread(Type)   assert(ThreadLocalStorage::GetThreadLocalStorage().GetType() == Type)


// Jani
JaniNamespaceEnd(Jani)

///////////////////////////////
namespace nonstd { // NONSTD //
///////////////////////////////

    template <class T>
    struct thread_local_frame_allocator
    {
        typedef T value_type;
        typedef std::true_type propagate_on_container_move_assignment;
        typedef std::true_type is_always_equal;

        thread_local_frame_allocator()  = default;
        ~thread_local_frame_allocator() = default;

        template <class U> constexpr thread_local_frame_allocator(const thread_local_frame_allocator<U>&) noexcept {}
        static [[nodiscard]] T* allocate(std::size_t n)
        {
            auto& thread_local_storage         = Jani::ThreadLocalStorage::GetThreadLocalStorage();
            auto& thread_local_frame_allocator = thread_local_storage.GetFrameAllocator();

#ifdef WITH_EDITOR
           
            // Request more data to store the unique index of the thread to assert invalid usage
            auto thread_unique_index = thread_local_storage.GetUniqueIndex();
            if (auto p = (T*)(thread_local_frame_allocator.allocate(sizeof(T) * n + sizeof(thread_unique_index), nullptr)))
            {
                std::memcpy(p, &thread_unique_index, sizeof(thread_unique_index));
                p = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(p) + sizeof(thread_unique_index));
                return p;
            }
#else
            // Allocate the data
            if (auto p = (T*)(thread_local_frame_allocator.allocate(sizeof(T) * n, nullptr))) return p;
#endif
            throw std::bad_alloc();
        }

        static void deallocate(T* p, std::size_t) noexcept
        {
            // Not necessary to deallocate the data since its lifetime is linked to the current frame and it will be
            // automatically reseted by its end

#ifdef WITH_EDITOR

            // Ensure we are trying to deallocate data on the same thread that allocated it first
            auto current_thread_unique_index                                 = Wonderland::ThreadLocalStorage::GetThreadLocalStorage().GetUniqueIndex();
            decltype(current_thread_unique_index) stored_thread_unique_index = *reinterpret_cast<decltype(current_thread_unique_index)*>(reinterpret_cast<uint8_t*>(p) - sizeof(current_thread_unique_index));
            assert(stored_thread_unique_index == current_thread_unique_index);
#endif
        }

        // The deleter method
        void operator()(T* b)
        {
            // Not necessary to deallocate the data since its lifetime is linked to the current frame and it will be
            // automatically reseted by its end

#ifdef WITH_EDITOR

            // Ensure we are trying to deallocate data on the same thread that allocated it first
            auto current_thread_unique_index                                 = Wonderland::ThreadLocalStorage::GetThreadLocalStorage().GetUniqueIndex();
            decltype(current_thread_unique_index) stored_thread_unique_index = *reinterpret_cast<decltype(current_thread_unique_index)*>(reinterpret_cast<uint8_t*>(b) - sizeof(current_thread_unique_index));
            assert(stored_thread_unique_index == current_thread_unique_index);
#endif
        }
    };
    template <class T, class U>
    bool operator==(const thread_local_frame_allocator<T>&, const thread_local_frame_allocator<U>&) { return true; }
    template <class T, class U>
    bool operator!=(const thread_local_frame_allocator<T>&, const thread_local_frame_allocator<U>&) { return false; }

    /*
     * These types of classes use frame memory from the current thread, its data will only exist during the current frame
     * (be it a game frame or render frame, its thread dependent)
     * Its contents can be copied into any other container if prolonged storage is needed
     * As an obvious information, never store this class in an object that has a lifetime higher than the frame that
     * is being processed
     */
    template <class _Ty>
    using transient_vector = std::vector<_Ty, thread_local_frame_allocator<_Ty>>;

    template <class _Kty, class _Ty, class _Pr = std::less<_Kty>>
    using transient_map = std::map<_Kty, _Ty, _Pr, thread_local_frame_allocator<std::pair<const _Kty, _Ty>>>;

    template <class _Kty, class _Ty, class _Hasher = std::hash<_Kty>, class _Keyeq = std::equal_to<_Kty>>
    using transient_unordered_map = std::unordered_map<_Kty, _Ty, _Hasher, _Keyeq, thread_local_frame_allocator<std::pair<const _Kty, _Ty>>>;

    template <class _Kty, class _Pr = std::less<_Kty>>
    using transient_set = std::set<_Kty, _Pr, thread_local_frame_allocator<_Kty>>;

    template <class _Kty, class _Hasher = std::hash<_Kty>, class _Keyeq = std::equal_to<_Kty>>
    using transient_unordered_set = std::unordered_set<_Kty, _Hasher, _Keyeq, thread_local_frame_allocator<_Kty>>;

    typedef std::basic_string<char, std::char_traits<char>, thread_local_frame_allocator<char>>          transient_string;
    typedef std::basic_string<wchar_t, std::char_traits<wchar_t>, thread_local_frame_allocator<wchar_t>> transient_wstring;

//////////////
} // NONSTD //
//////////////