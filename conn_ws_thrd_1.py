import websocket, os, time, pickle
from datetime import datetime
import threading
from collections import deque
from EQdetect.utils.vorstat import voronoi_sta
from EQdetect.utils.config import Config
from EQdetect.utils.report import report
from EQdetect.core.sourcecal import Phase, EQsrc, pending_eq
import numpy as np
import posix_ipc as ipc
import sys
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
    global received_data
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
                    received_data.append(data)
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

def check_eq(eq1, eq2):
    src1 = eq1.optsrc
    src2 = eq2.optsrc
    if ((abs(src1[0]-src2[0])<=0.5 and abs(src1[1]-src2[1])<=0.5 and (abs(src1[4]-src2[4])<=10))) or (abs(src1[0]-src2[0])<=0.4 and abs(src1[1]-src2[1])<=0.4 and (abs(src1[4]-src2[4])<=60)):
        return True
    elif eq1.status == 0 and eq1.check_trgg(eq2.first):
        return True
    elif eq1.status >= 1 and eq1.check_estg(eq2.first):
        return True
    else:
        return False

def main():
    print(os.getcwd())
    # global variable of receiving data from webscoket
    global received_data
    received_data = deque(maxlen=100)

    getpick_thread = threading.Thread(target=reader_task)
    getpick_thread.start()

    last_pick = []
    cfg = Config()
    vorcel = voronoi_sta()
    vorcel.update_trig()
    picking = []
    noeq = 0
    eqp = []
    eqonl = []
    last_report_time = time.time() - 120
    while True:
        #Pending Earthquake
        tmp_eqp = []
        for eqp_l in eqp:
            eqp_l.eq_stat(vorcel)
            if len(eqp_l.rmphase) != 0:
                for rmpick in eqp_l.rmphase:
                    picking.append(rmpick)
                eqp_l.rmphase = []
            if eqp_l.status == -1:
                print("Remove of Pending eartquake ", eqp_l.starttime)
                continue
            elif eqp_l.status >= 1:
                eqonl.append(eqp_l)
                continue
            if eqp_l.status==0:                             
                tmp_eqp.append(eqp_l)
        eqp = tmp_eqp
        
        #Merge Event
        for i in range(len(eqonl)):
            if eqonl[i].status == -1:
                continue
            for j in range(len(eqonl)):
                if j>i:
                    if eqonl[j].status == -1:
                        continue
                    if eqonl[i].noeq == eqonl[j].noeq:
                        continue
                    status = check_eq(eqonl[i], eqonl[j])
                    if status:
                        print(eqonl[i].noeq, eqonl[j].noeq, i, j)
                        eqonl[j].status = -1
                        eqonl[j].rmphase = []
                        for k in range(len(eqonl[j].phase)):
                            eqonl[i].phase.append(eqonl[j].phase[k])                

        tmp_eqon = []            
        for eqon in eqonl:
            if eqon.status == -1:
                print("Remove of On Going eartquake ", eqon.starttime)
                continue
            eqon.eq_stat(vorcel)
            if len(eqon.rmphase) != 0:
                # for rmpick in eqon.rmphase:
                #     picking.append(rmpick)
                eqon.rmphase = []
            if eqon.status == -1:
                print("Remove of On Going eartquake ", eqon.starttime)
                continue
            elif eqon.status == 1:
                if eqon.update<=50:
                    if (time.time() - eqon.endtime.timestamp() - 25200) >= 0.45 or eqon.update < 1:
                        eqon.cal_hypo(vorcel)
                        eqon.update = eqon.update + 1
                        eqon.endtime = datetime.utcnow() 
                        if eqon.update > 1:
                            try:
                                file = open("output1.txt", "a")
                                file.write(
                                    f"EQ detected No {eqon.noeq} Update {eqon.update} with paramater {eqon.starttime} {datetime.utcnow()} \n")
                                file.write(
                                    f"Longitude : {round(eqon.optsrc[0],3)}+-{round(eqon.optsrc[5]*111.1,3)}\nLatitude : {round(eqon.optsrc[1],3)}+-{round(eqon.optsrc[6]*111.1,3)}\nDepth : {round(eqon.optsrc[2],5)}+-{round(eqon.optsrc[7],3)}\nOT : {round(eqon.optsrc[4],2)}\nMag : {round(eqon.optsrc[3],2)}\n")
                                file.write(f"First Trigger : {eqon.first.sta}\n")
                                file.write(f"Count Phase : {len(eqon.phase)}\n")
                                file.write(
                                    f"Resampling : {eqon.need_resample}\n\n")
                            except Exception as e:
                                print(eqon.optsrc, eqon.noeq)
                                raise ValueError(e)
                            
                            if eqon.update > 6:
                                diffs = abs(np.diff(np.array(eqon.var), axis=0))
                                converged = np.sqrt(diffs[-1][0]**2+diffs[-1][1]**2)*111.1
                                if converged < 1.0:
                                    if eqon.update >= 50:
                                        eqon.status = 2
                                        file.write(
                                            "The values in the array converge.\n\n")
                                    else:
                                        file.write(
                                            "The values in the array do not converge.\n\n")
                            file.close()
                            if len(eqon.phase) >= 3:
                                if len(eqon.phase) >= 4 and eqon.update >= 2 and eqon.report is False:
                                    eqon.report = True
                                if eqon.report is True:
                                    eqon.report = 1
                                    if time.time()-last_report_time>120:
                                        otp = datetime.utcfromtimestamp(eqon.optsrc[4]).strftime('%Y-%m-%d %H:%M:%S')
                                        pesan = (
                                                f"Peringatan Dini Gempa Bumi (MulEW Method)\nOrigin Time : {otp} UTC\n"
                                                f"Longitude : {round(eqon.optsrc[0],3)} +- {round(eqon.optsrc[5],3)}\n"
                                                f"Latitude : {round(eqon.optsrc[1],3)} +- {round(eqon.optsrc[6],3)}\n"
                                                f"Depth : {round(eqon.optsrc[2],5)} km\n"
                                                f"Mag(Mpd) : {round(eqon.optsrc[3],2)}\n"
                                                f"Hasil merupakan update ke {eqon.update}"
                                        )
                                        last_report_time = time.time()  
                                trp = datetime.utcnow().strftime("%Y%m%d%H%M%S")
                                outf = f"./eew_report/{eqon.noeq}_{trp}_{eqon.update}.pkl"
                                with open(outf, "wb") as filea:
                                    pickle.dump(eqon, filea)
                                print(eqon.noeq, eqon.optsrc[:6], eqon.endtime, datetime.utcnow(), time.time() - eqon.endtime.timestamp() - 25200)  
                    #else:
                    #    print(time.time() - eqon.endtime.timestamp() - 25200)
            if eqon.status >= 1:                                 
                tmp_eqon.append(eqon)
        eqonl = tmp_eqon
        
        if len(list(received_data)) == 0:
            # time.sleep(0.1)
            continue
        data_pick = list(received_data.copy())

        #Picking Checking
        for pick in data_pick:
            if pick in last_pick or float(pick[13]) > 4:
                continue
            # print(pick)
            new_pick = Phase(pick)
            if new_pick.check_pick(vorcel):
                picking.append(new_pick)
        last_pick = data_pick

        #Picking Processing
        for pick in picking:
            if not eqp and not eqonl:
                if pick.set_first(vorcel):
                    noeq += 1
                    allow_eq, eqpl = pending_eq(EQsrc(pick, cfg, noeq),vorcel)
                    if allow_eq:
                        eqp.append(eqpl)
                continue
            
            statpick = False
            for eqon in eqonl:
                # pick.calerror(eqon.optsrc)
                if eqon.status >= 1:
                    if eqon.check_estg(pick) is True:
                        statpick = True
                        if eqon.status == 2:
                            print(pick.sta, "Late Pick with", eqon.first.sta)
                        print(pick.sta, "estgroup with", eqon.first.sta, eqon.starttime, datetime.utcnow())
                        break
                    # elif pick.sta in eqp_l.estgroup: 
                    #     print(pick.sta, "estgroup with", eqon.first.sta, "But Big error")
                    # else:
                    #     print(pick.sta, "Not estgroup with", eqon.first.sta)
                            
            if statpick:
                continue
             
            for eqp_l in eqp:
                if eqp_l.status == 0 and not statpick:
                    # pick.calerror(eqp_l.optsrc)
                    if eqp_l.check_trgg(pick):
                        statpick = True
                        print(pick.sta, "triggroup with", eqp_l.first.sta, pick.reserr, eqp_l.starttime, datetime.utcnow())
                        break
                    # elif pick.sta in eqp_l.triggroup: 
                    #     print(pick.sta, "Not triggroup with", eqp_l.first.sta, pick.reserr, eqp_l.endtime, eqp_l.starttime)

            if statpick:
                continue
            
            if pick.set_first(vorcel):
                noeq += 1
                allow_eq, eqpl = pending_eq(EQsrc(pick, cfg, noeq),vorcel)
                if allow_eq:
                    if len(eqp) >= 30:
                        eqp.pop(0)  
                    eqp.append(eqpl)
        picking = []
if __name__ == "__main__":
    main()