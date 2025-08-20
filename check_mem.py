import posix_ipc as ipc
import sys

def cleanup_resources():
    try:
        ipc.unlink_semaphore("/my_sem")
        print("Semafor /my_sem berhasil dihapus.")
    except ipc.ExistentialError:
        print("Semafor /my_sem tidak ditemukan.")
    
    try:
        ipc.unlink_shared_memory("/my_shm")
        print("Shared memory /my_shm berhasil dihapus.")
    except ipc.ExistentialError:
        print("Shared memory /my_shm tidak ditemukan.")

if __name__ == "__main__":
    cleanup_resources()
