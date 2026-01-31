#pragma once

#include <string.h>  
#include <stdlib.h>

#if defined (WIN32)
#   include <windows.h>
#   define MAP_NAME_PREFIX "Local\\" 
#   define INVALID_HANDLE_VALUE_WIN (NULL) 
#   define PLATFORM_SEMAPHORE_TYPE  HANDLE 
#else
#   include <sys/mman.h>   
#   include <sys/stat.h>  
#   include <fcntl.h>    
#   include <unistd.h>      
#   include <semaphore.h>    // semaphore POSIX
#   define HANDLE          int              
#   define INVALID_HANDLE_VALUE_WIN (-1)    
#   define MAP_NAME_PREFIX  "/"             
#   define PLATFORM_SEMAPHORE_TYPE  sem_t*  
#endif

#define SEMAPHORE_NAME_POSTFIX "_semaphore" 

namespace cplib
{
    template <class T> 
    class SharedMemory
    {
    public:

        SharedMemory(const char* object_name, bool create_if_not_exists = true)
            : file_descriptor_(INVALID_HANDLE_VALUE_WIN), 
              shared_memory_ptr_(NULL), 
              semaphore_handle_(NULL)
        {
            // memmory allocation
            memory_object_name_ = (char*)malloc(strlen(object_name) + strlen(MAP_NAME_PREFIX) + 1);
            memcpy(memory_object_name_, MAP_NAME_PREFIX, strlen(MAP_NAME_PREFIX));
            memcpy(memory_object_name_ + strlen(MAP_NAME_PREFIX), object_name, strlen(object_name)+1);
            
            //create semaphore
            semaphore_object_name_ = (char*)malloc(strlen(memory_object_name_) + strlen(SEMAPHORE_NAME_POSTFIX) + 1);
            memcpy(semaphore_object_name_, memory_object_name_, strlen(memory_object_name_));
            memcpy(semaphore_object_name_ + strlen(memory_object_name_), SEMAPHORE_NAME_POSTFIX, strlen(SEMAPHORE_NAME_POSTFIX) + 1);

            bool is_newly_created = false;
            bool operation_success = OpenExistingMemory(memory_object_name_, semaphore_object_name_);
            
            if (!operation_success && create_if_not_exists) {
                if (operation_success = CreateNewMemory(memory_object_name_, semaphore_object_name_))
                    is_newly_created = true;
            }
            //add to adr space
            if (operation_success)
                operation_success = MapMemoryToAddressSpace();
            
            if (operation_success && is_newly_created) {
                shared_memory_ptr_->client_count = 0;
                shared_memory_ptr_->data = T(); 
            }
            
            if (operation_success) {
                LockSemaphore();
                shared_memory_ptr_->client_count++;
                UnlockSemaphore();
            } else {
                if (is_newly_created)
                    DestroyMemoryObject();
                else
                    CloseMemoryResources(); 
            }
        }

        virtual ~SharedMemory() {
            if (IsValid()) {
                int remaining_clients = 0;
                LockSemaphore();
                shared_memory_ptr_->client_count--;
                remaining_clients = shared_memory_ptr_->client_count;
                UnlockSemaphore();
                
                if (remaining_clients <= 0)
                    DestroyMemoryObject();
                else
                    CloseMemoryResources(); 
            }
            
            free(memory_object_name_);
            free(semaphore_object_name_);
        }

        bool IsValid() {
            return file_descriptor_ != INVALID_HANDLE_VALUE_WIN && 
                   semaphore_handle_ != NULL && 
                   shared_memory_ptr_ != NULL;
        }

        void Lock() {
            LockSemaphore();
        }

        T* Data() {
            if (!IsValid())
                return NULL;
            return &shared_memory_ptr_->data;
        }

        void Unlock() {
            UnlockSemaphore();
        }

    private:
        struct SharedMemoryContent
        {
            T data;          
            int client_count; 
        };
        bool OpenExistingMemory(const char* memory_object_name, const char* semaphore_name) {
#if defined (WIN32)
            file_descriptor_ = OpenFileMapping(FILE_MAP_WRITE, true, memory_object_name);
            if (file_descriptor_ != INVALID_HANDLE_VALUE_WIN)
                semaphore_handle_ = OpenSemaphore(SEMAPHORE_ALL_ACCESS, false, semaphore_name);
#else
            file_descriptor_ = shm_open(memory_object_name, O_RDWR, 0644);
            if (file_descriptor_ != INVALID_HANDLE_VALUE_WIN) {
                semaphore_handle_ = sem_open(semaphore_name, 0);
                if (semaphore_handle_ == SEM_FAILED)
                    semaphore_handle_ = NULL;
            }
#endif
            return (file_descriptor_ != INVALID_HANDLE_VALUE_WIN && semaphore_handle_ != NULL);
        }

        bool CreateNewMemory(const char* memory_object_name, const char* semaphore_name) {
#if defined (WIN32)
            file_descriptor_ = CreateFileMapping(
                INVALID_HANDLE_VALUE, 
                NULL, 
                PAGE_READWRITE, 
                0, 
                sizeof(SharedMemoryContent), 
                memory_object_name
            );
            if (file_descriptor_ != INVALID_HANDLE_VALUE_WIN)
                semaphore_handle_ = CreateSemaphore(NULL, 0, 1, semaphore_name);
#else
            file_descriptor_ = shm_open(memory_object_name, O_CREAT | O_EXCL | O_RDWR, 0644);
            if (file_descriptor_ != INVALID_HANDLE_VALUE_WIN) {
                ftruncate(file_descriptor_, sizeof(SharedMemoryContent));
                semaphore_handle_ = sem_open(semaphore_name, O_CREAT | O_EXCL, 0644, 1);
                if (semaphore_handle_ == SEM_FAILED)
                    semaphore_handle_ = NULL;
            }
#endif
            return (file_descriptor_ != INVALID_HANDLE_VALUE_WIN && semaphore_handle_ != NULL);
        }

        bool MapMemoryToAddressSpace() {
            if (file_descriptor_ == INVALID_HANDLE_VALUE_WIN)
                return false;
                
#if defined (WIN32)
            shared_memory_ptr_ = reinterpret_cast<SharedMemoryContent*>(
                MapViewOfFile(file_descriptor_, FILE_MAP_WRITE, 0, 0, sizeof(SharedMemoryContent))
            );
#else
            void* mapping_result = mmap(
                NULL, 
                sizeof(SharedMemoryContent), 
                PROT_WRITE | PROT_READ, 
                MAP_SHARED, 
                file_descriptor_, 
                0
            );
            if (mapping_result == MAP_FAILED)
                shared_memory_ptr_ = NULL;
            else
                shared_memory_ptr_ = reinterpret_cast<SharedMemoryContent*>(mapping_result);
#endif
            return (shared_memory_ptr_ != NULL);
        }

        bool UnmapMemoryFromAddressSpace() {
            if (shared_memory_ptr_ == NULL)
                return false;
                
#if defined (WIN32)
            UnmapViewOfFile(shared_memory_ptr_);
#else
            munmap(shared_memory_ptr_, sizeof(SharedMemoryContent));
#endif
            shared_memory_ptr_ = NULL;
            return true;
        }

        void CloseMemoryResources() {
            UnmapMemoryFromAddressSpace();
            
            if (file_descriptor_ != INVALID_HANDLE_VALUE_WIN) {
#if defined (WIN32)
                CloseHandle(file_descriptor_);
#else
                close(file_descriptor_);
#endif		
                file_descriptor_ = INVALID_HANDLE_VALUE_WIN;
            }
            
            if (semaphore_handle_ != NULL) {
#if defined (WIN32)
                CloseHandle(semaphore_handle_);
#else
                sem_close(semaphore_handle_);
#endif
                semaphore_handle_ = NULL;
            }
        }

        void DestroyMemoryObject()
        {
            CloseMemoryResources();
            
            // manual delete in POSIX
#if !defined (WIN32)
            shm_unlink(memory_object_name_);
            sem_unlink(semaphore_object_name_);
#endif
        }

        void LockSemaphore()
        {
#if defined (WIN32)
            ReleaseSemaphore(semaphore_handle_, 1, NULL);
#else
            sem_post(semaphore_handle_);
#endif
        }

        void UnlockSemaphore()
        {
#if defined (WIN32)
            WaitForSingleObject(semaphore_handle_, 0);
#else
            sem_wait(semaphore_handle_);
#endif
        }
        
        SharedMemoryContent* shared_memory_ptr_; 
        PLATFORM_SEMAPHORE_TYPE semaphore_handle_;
        HANDLE file_descriptor_;
        char* memory_object_name_;
        char* semaphore_object_name_;
    };
} 