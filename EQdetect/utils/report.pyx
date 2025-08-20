from datetime import datetime
import telegram, json, pickle
import asyncio, requests
from cython cimport boundscheck, wraparound
asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())

async def send_msg(pesan):
    bot = telegram.Bot(token='5298253276:AAEThCM6SPGQvNS4uM4NjCDNh44oh_07QBU')
    try:
        pesan = pesan.decode('utf-8')
        await bot.send_message(chat_id=-1001775156336, text=pesan)
        print("Message sent successfully")
    except Exception as e:
        print(f"An error occurred: {e}")

async def pushsimap(data):
    payload = {
        "originTime": data[0],
        "sent": data[1],
        "epicenterLon": data[2],
        "epicenterLat": data[3],
        "depth": data[4],
        "magnitude": data[5],
        "identifier": data[6],
        "senderName": "AculCorp",
        "references": "BMKG-EEW"
    }
    json_payload = json.dumps(payload)
    headers = {
        "Content-Type": "application/json"
    }
    try:
        response = requests.post("https://event-eews.seismon.my.id/postevent", data=json_payload, headers=headers, timeout=5.0)
        try:
            res = response.json()
        except:
            try:
                res = response.content.decode()
            except:
                res = "Kesalahan Decode"
        if response.status_code == 200:
            print(response.status_code, res['err'])
        else:
            print(response.status_code, res)                
    except requests.exceptions.RequestException as e:
        print("Error", e)

@boundscheck(False)
@wraparound(False)
cpdef report_1(eqsrc, path, status):
    cdef str trp = datetime.utcnow().strftime("%Y%m%d%H%M%S")
    cdef unicode outf = f"{path}/{eqsrc.noeq}_{trp}_{eqsrc.update}.dat"
    cdef str otp = datetime.utcfromtimestamp(eqsrc.optsrc[4]).strftime('%Y-%m-%d %H:%M:%S')

    cdef unicode data = (
        f"Earthquake No {eqsrc.noeq} report on {eqsrc.endtime}\n"
        "Optimal Hypocenter\n"
        "OriginTime    Long    Lat    Depth    Mag(Mpd)    Varlong    Varlat    Vardepth RMSE\n"
        f"{otp} {round(eqsrc.optsrc[0],3)} {round(eqsrc.optsrc[1],3)} {round(eqsrc.optsrc[2],5)} {round(eqsrc.optsrc[3],2)} {round(eqsrc.optsrc[5],3)} {round(eqsrc.optsrc[6],3)} {round(eqsrc.optsrc[7],3)} {round(eqsrc.rmse,3)}\n"
        f"First Trigger : {eqsrc.first.sta} {eqsrc.triggroup}\n"
        f"Trigger Group : {eqsrc.triggroup}\n"
        f"Estimat Group : {eqsrc.estgroup}\n"
        "Sta    Lon   Lat   Parr   Perr   pa    pv    pd   wei\n"
    )

    cdef unicode picks = "\n".join([
        f"{pick.sta} {pick.longitude} {pick.latitude} {pick.picktime} {pick.reserr} {pick.pa} {pick.pv} {pick.pd} {pick.weight}"
        for pick in eqsrc.phase
    ])

    cdef unicode particles = "\n".join([
        f"{pf[4]} {round(pf[0],5)} {round(pf[1],5)} {round(pf[2],5)} {pf[3]} {pf[5]}"
        for pf in eqsrc.particle
    ])

    cdef bytes pesan = (
        f"Peringatan Dini Gempa Bumi (MulEW Method)\nOrigin Time : {otp} UTC\n"
        f"Longitude : {round(eqsrc.optsrc[0],3)} +- {round(eqsrc.optsrc[5],3)}\n"
        f"Latitude : {round(eqsrc.optsrc[1],3)} +- {round(eqsrc.optsrc[6],3)}\n"
        f"Depth : {round(eqsrc.optsrc[2],5)} km\n"
        f"Mag(Mpd) : {round(eqsrc.optsrc[3],2)}\n"
        f"Hasil merupakan update ke {eqsrc.update}"
    ).encode("utf-8")

    with open(outf, "w") as file:
        file.write(data)
        file.write(picks)
        file.write("\nOriginTime    Long    Lat    Depth    Mag   Weight\n")
        file.write(particles)

    if status is False:
        print("Send TELE")
        asyncio.run(send_msg(pesan))
        #data = [otp.strftime('%Y-%m-%d %H:%M:%S'), trp, round(eqsrc.optsrc[0],3), round(eqsrc.optsrc[1],3), round(eqsrc.optsrc[2],5), round(eqsrc.optsrc[3],2), eqsrc.noeq]
        #asyncio.run(pushsimap(data))

@boundscheck(False)
@wraparound(False)
cpdef report(eqsrc, path, status):
    cdef bytes pesan
    cdef str trp = datetime.utcnow().strftime("%Y%m%d%H%M%S")
    cdef unicode outf = f"{path}/{eqsrc.noeq}_{trp}_{eqsrc.update}.dat"
    cdef str otp

    with open(outf, "wb") as file:
        pickle.dump(eqsrc, file)

    if status is False:
        otp = datetime.utcfromtimestamp(eqsrc.optsrc[4]).strftime('%Y-%m-%d %H:%M:%S')
        pesan = (
            f"Peringatan Dini Gempa Bumi (MulEW Method)\nOrigin Time : {otp} UTC\n"
            f"Longitude : {round(eqsrc.optsrc[0],3)} +- {round(eqsrc.optsrc[5],3)}\n"
            f"Latitude : {round(eqsrc.optsrc[1],3)} +- {round(eqsrc.optsrc[6],3)}\n"
            f"Depth : {round(eqsrc.optsrc[2],5)} km\n"
            f"Mag(Mpd) : {round(eqsrc.optsrc[3],2)}\n"
            f"Hasil merupakan update ke {eqsrc.update}"
        ).encode("utf-8")

        print("Send TELE")
        asyncio.run(send_msg(pesan))
        #data = [otp.strftime('%Y-%m-%d %H:%M:%S'), trp, round(eqsrc.optsrc[0],3), round(eqsrc.optsrc[1],3), round(eqsrc.optsrc[2],5), round(eqsrc.optsrc[3],2), eqsrc.noeq]
        #asyncio.run(pushsimap(data))