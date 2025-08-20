import posix_ipc as ipc
import sys
import time
import os
import mmap

# Definisikan nama shared memory, semafor, dan ukuran yang sama dengan program C
SHM_NAME = "/my_shm"
SEM_NAME = "/my_sem"
SHM_SIZE = 4096

def reader_task():
    """
    Fungsi ini membaca data dari shared memory menggunakan semafor sebagai sinyal.
    """
    sem = None
    shm_block = None
    try:
        # Buka semafor dan shared memory yang sudah dibuat oleh program C
        # Buka dengan O_CREAT dan nilai awal 1
        sem = ipc.Semaphore(SEM_NAME, ipc.O_CREAT, initial_value=1)
        # Gunakan posix_ipc untuk shared memory
        shm_block = ipc.SharedMemory(name=SHM_NAME)
        last_counter = -1
        
        # Mmap shared memory untuk mendapatkan objek buffer yang dapat diakses
        shm_map = mmap.mmap(shm_block.fd, SHM_SIZE)

        print("Reader Task: Started and ready to read from shared memory (semaphore mode).")

        while True:
            try:
                # Tunggu semaphore dari C (blocking, jadi sinkron)
                sem.acquire()

                # Setelah semaphore diberikan oleh C -> baca data
                current_counter = int.from_bytes(shm_map[:4], byteorder='little')

                if current_counter > last_counter:
                    message_bytes = shm_map[4:]
                    data = message_bytes.decode('utf-8').split('\x00', 1)[0]

                    print(f"Reader Task: Found new data (Counter: {current_counter}). Data: '{data}'")
                    last_counter = current_counter

                # ⚠️ Jangan sem.release() di reader!
                # Karena sem_post hanya dilakukan di writer (program C)

                time.sleep(0.1)

            except Exception as e:
                print(f"Reader Task: error {e}")
                break
            
    except (FileNotFoundError, ipc.ExistentialError):
        print("Reader Task: Shared memory or semaphore not found. Exiting.")
    except Exception as e:
        print(f"Reader Task error: {e}")
    finally:
        if shm_block:
            # Tutup memory map
            shm_map.close()
            # Tutup file descriptor shared memory
            os.close(shm_block.fd)
        if sem:
            sem.close()
        print("Reader Task: Exiting.")

if __name__ == "__main__":
    reader_task()
