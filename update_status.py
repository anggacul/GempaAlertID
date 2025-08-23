from EQdetect.utils.vorstat import voronoi_sta
import mysql.connector
import os
import pandas as pd
# os.chdir(r"C:\Users\wijay\Documents\EEWS_new\eventbaru")
import glob
config = {
    'host': 'localhost',
    'user': 'root',
    'database': 'ipfeew'
}
config1 = {
    'host': '202.90.199.206',
    'user':'server',
    'password':'bmkgaccelero',
    'database': 'db_simora'
}
mydb = mysql.connector.connect(**config)
mycursor = mydb.cursor()
mycursor.execute(f"SELECT Kode, Latitude, Longitude, status FROM meta_playback WHERE `Kode` NOT LIKE '%Virtual%'")
station = mycursor.fetchall()
# df = pd.DataFrame(station, columns=["Kode", "Lat", "Long"])

for i, sta in enumerate(station):
    station[i] = list(station[i])
    files = glob.glob(f"20240225T130705_105.87_-7.49_6.0/{sta[0]}_Z.mseed")
    if files:
        station[i][-1] = 1
    else:
        station[i][-1] = 0
    # if sta[6] == "ON":
    #     station[i][6] = 1
    # else:
    #     station[i][6] = 0

# file = open("data_sta.txt", "r")
# data = file.readlines()
# file.close()

# for i in range(len(data)):
#     data[i] = data[i].split()
# print(len(data))
#get station status from 
# mydb = mysql.connector.connect(**config1)
# mycursor = mydb.cursor()
# mycursor.execute(f"SELECT Kode, Lat, `Long`, Status_tele FROM tb_avaccelro where Tipe LIKE '%Accelerograph%'")
# station = mycursor.fetchall()
vorcel = voronoi_sta()
# latvir = range(-14,11,2)
# longvir = range(90,143,2)
# k = 0
# station = []
# for i in latvir:
#     if i == latvir[0] or i == latvir[-1]:
#     for j in longvir:
#         code = "Virtual"+str(k)
#         station.append((code, i, j, 1))
#         k += 1
#     else:
#     code = "Virtual"+str(k)
#     k += 1
#     station.append((code, i, longvir[0], 1))
#     code = "Virtual"+str(k)
#     k += 1
#     station.append((code, i, longvir[-1], 1))
# for i, sta in enumerate(station):
#     sta = list(sta)
#     print(sta)
#     if sta[-1] == "ON":
#     sta[-1] = 1
#     else:
#     sta[-1] = 0
#     station[i] = sta
vorcel.insert_sta(station)
vorcel.update_trig()
# idx = vorcel.code.index("WAMI")
# print(len(vorcel.triggroup), len(vorcel.code), len(vorcel.estgroup), len(vorcel.init))
# point_trig = vorcel.triggroup[idx]

# def plot_vor_sta(self, kode):
#     idx = self.code.index(kode)
#     point_sta = self.points[idx]
#     point_trig = self.triggroup[idx]
#     point_est = self.estgroup[idx]
#     init_eq = self.init[idx]
#     point_cent = self.cen_p[idx]
#     fig, ax = plt.subplots(figsize=(8,12))
#     world = geopandas.read_file(r'C:\Users\wijay\Documents\EEWS_new\ne_10m_land.zip')
#     world = world.cx[94:120, -10:14]
#     world.plot(ax=ax, alpha=0.4, color="grey")
#     for i, region in enumerate(self.regions):
#         polygon = self.vertices[region]
#         ax.plot(*zip(*polygon), 'k', linewidth=0.5)
#     for i, p in enumerate(self.points):
#         ax.plot(p[0], p[1], 'k^')
#     for i, p in enumerate(point_est):
#         ax.plot(p[0], p[1], 'b^')
#     for i, p in enumerate(point_trig):
#         ax.plot(p[0], p[1], 'r^')
#     ax.plot(point_cent[0], point_cent[1], 'r*', linewidth=3.0)
#     ax.plot(point_sta[0], point_sta[1], 'g^', linewidth=3.0)
#     ax.plot(*zip(*init_eq), 'r', linewidth=0.5)
#     ax.set_xlim([85, 150])
#     ax.set_ylim([-15, 14])
#     plt.show()