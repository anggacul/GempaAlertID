import multiprocessing as mp
import multiprocessing.shared_memory as shm
import posix_ipc as ipc
import sys
import time

# Definisikan nama shared memory, semafor, dan ukuran
SHM_NAME = "/my_shm"
SEM_NAME = "/my_sem"
SHM_SIZE = 1024

class SharedData:
    def __init__(self, buf):
        self.buf = buf

    def get_counter(self):
        return int.from_bytes(self.buf[:4], byteorder='little')

    def get_data(self):
        message_bytes = self.buf[4:]
        return message_bytes.decode('utf-8').split('\x00', 1)[0]

def reader_task(data_queue, stop_event):
    """
    Fungsi ini membaca data dari shared memory dan memasukkannya ke queue.
    """
    sem = None
    shm_block = None
    try:
        # Buka semafor dan shared memory. Ini akan gagal jika producer belum berjalan.
        sem = ipc.Semaphore(SEM_NAME)
        shm_block = shm.SharedMemory(name=SHM_NAME)
        shared_data = SharedData(shm_block.buf)
        last_counter = -1
        
        print("Reader Task: Started and ready to read from shared memory.")

        while not stop_event.is_set():
            sem.acquire(timeout=1)
            
            # Cek apakah produsen telah memberi sinyal data baru
            if sem.get_value() > 0:
                current_counter = shared_data.get_counter()
                if current_counter > last_counter:
                    data = shared_data.get_data()
                    print(f"Reader Task: Found new data (Counter: {current_counter}). Putting into queue...")
                    # put() akan memblokir jika queue penuh (maxsize=100)
                    data_queue.put(data)
                    last_counter = current_counter
            
            time.sleep(0.1)

    except (FileNotFoundError, ipc.ExistentialError):
        print("Reader Task: Shared memory or semaphore not found. Signaling other processes to stop.")
        stop_event.set()
    except Exception as e:
        print(f"Reader Task error: {e}")
        stop_event.set()
    finally:
        if shm_block:
            shm_block.close()
        if sem:
            sem.close()
        print("Reader Task: Exiting.")

def processor_task(data_queue, stop_event):
    """
    Fungsi ini mengambil SEMUA data dari queue dan memprosesnya dalam batch.
    """
    print("Processor Task: Started and waiting for data in the queue.")
    while not stop_event.is_set():
        try:
            # Mengambil semua data dari queue dan menyimpannya di list
            all_data = []
            while not data_queue.empty():
                all_data.append(data_queue.get())
            
            # Memproses setiap item yang dikumpulkan
            if all_data:
                print(f"\nProcessor Task: Processing a batch of {len(all_data)} items.")
                for item in all_data:
                    print(f"Processor Task: Processing data: '{item}'")
                    time.sleep(0.1)
                
            # --- Bagian untuk 'proses lain' Anda di sini ---
            print("Processor Task: Performing other tasks...")
            time.sleep(1)

        except Exception as e:
            print(f"Processor Task error: {e}")
            break

    print("Processor Task: Exiting.")

if __name__ == "__main__":
    with mp.Manager() as manager:
        data_queue = manager.Queue(maxsize=100)
        stop_event = manager.Event()
        
        reader_process = mp.Process(target=reader_task, args=(data_queue, stop_event))
        processor_process = mp.Process(target=processor_task, args=(data_queue, stop_event))
        
        reader_process.start()
        processor_process.start()
        
        reader_process.join()
        processor_process.join()