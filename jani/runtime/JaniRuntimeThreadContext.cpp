////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntimeThreadContext.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniRuntimeThreadContext.h"

///////////////////////////
namespace Jani { // JANI //
///////////////////////////

    thread_local ThreadLocalStorage  s_thread_local_storage;
    std::vector<ThreadLocalStorage*> s_all_thread_local_storages;
    std::mutex                       s_all_thread_local_storages_mutex;

    ThreadLocalStorage& ThreadLocalStorage::GetThreadLocalStorage()
    {
        return s_thread_local_storage;
    }

    ThreadLocalStorage::GlobalThreadStoragesWrapper ThreadLocalStorage::GetAllThreadLocalStorages()
    {
        return GlobalThreadStoragesWrapper(s_all_thread_local_storages, s_all_thread_local_storages_mutex);
    }

    void ThreadLocalStorage::AcknowledgeFrameEnd()
    { 
        std::lock_guard l(s_all_thread_local_storages_mutex);
        for (auto& thread_context : s_all_thread_local_storages)
        {
            thread_context->m_frame_allocator->reset();
        }
    }

    ThreadLocalStorage::ThreadLocalStorage()
    {
        static uint32_t thread_index_counter = 0;

        m_frame_allocator = std::make_unique<nonstd::frame_block_allocator<>>();
        m_unique_index    = thread_index_counter++;

        {
            std::lock_guard l(s_all_thread_local_storages_mutex);
            s_all_thread_local_storages.push_back(this);
        }
    }

    ThreadLocalStorage::~ThreadLocalStorage()
    {
        std::lock_guard l(s_all_thread_local_storages_mutex);

        auto iter = std::find(s_all_thread_local_storages.begin(), s_all_thread_local_storages.end(), &s_thread_local_storage);
        if (iter != s_all_thread_local_storages.end())
        {
            s_all_thread_local_storages.erase(iter);
        }
    }

    uint32_t ThreadLocalStorage::GetUniqueIndex() const
    {
        return m_unique_index;
    }

    nonstd::frame_block_allocator<>& ThreadLocalStorage::GetFrameAllocator() const
    {
        return *m_frame_allocator;
    }

/////////////
}; // JANI //
/////////////