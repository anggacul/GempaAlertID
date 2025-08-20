# from EQdetect.utils.update_db import upd_db
from EQdetect.utils.vorstat import voronoi_sta
from datetime import datetime, timedelta, timezone
import time, pickle, geojson
import mysql.connector

def upd_db():

    config = {
        'host': 'localhost',
        'user':'root',
        'database': 'ipfeew'
    }

    config1 = {
        'host': '202.90.199.206',
        'user':'server',
        'password':'bmkgaccelero',
        'database': 'db_simora'
    }

    #get station status from 
    mydb = mysql.connector.connect(**config1)
    mycursor = mydb.cursor()
    mycursor.execute(f"SELECT Kode, Last_data FROM tb_avaccelro where Tipe LIKE '%Accelerograph%'")
    station = mycursor.fetchall()
    mydb.commit()
    mydb.close()
    t0 = datetime.utcnow()
    mydb = mysql.connector.connect(**config)
    mycursor = mydb.cursor()
    for sta in station:
        t1 = datetime.strptime(sta[1],"%Y-%m-%dT%H:%M:%S")
        diff = (t0-t1).total_seconds()
        if diff > 30*60:
            # print(sta[0], "OFF")
            sql = f"UPDATE meta_playback SET status=0 WHERE Kode='{sta[0]}'"
        else:
            # print(sta[0], "ON")
            sql = f"UPDATE meta_playback SET status=1 WHERE Kode='{sta[0]}'"
        mycursor.execute(sql)
        mydb.commit()
    mydb.close()

t0 = datetime.utcnow() - timedelta(seconds=6*60)
while True:
    diff = datetime.utcnow()-t0
    if diff.total_seconds() > 5*60:
        a=time.time()
        upd_db()
        try:
            vorcel = voronoi_sta()
            rowupdate = vorcel.update_trig()
            if rowupdate > 0:
                print("New Voronoi Cell")
                filename = f"vorcel/{t0.strftime('%y%m%d%H%M%S')}_vorcel.pkl"
                with open(filename, 'wb') as outp:
                    pickle.dump(vorcel, outp, pickle.HIGHEST_PROTOCOL)
                features = []
                features_point = []
                for region, coord, code in zip(vorcel.regions, vorcel.points, vorcel.code):
                    if code[:3] == 'Vir':
                        continue
                    polygon = vorcel.vertices[region]
                    polygon = geojson.Polygon([polygon.tolist()])
                    feature = geojson.Feature(geometry=polygon)
                    features.append(feature)
                    point = geojson.Point(coord)
                    properties = {"code": code}  # Add code property
                    feature1 = geojson.Feature(geometry=point, properties=properties)
                    features_point.append(feature1)
                    
                feature_collection = geojson.FeatureCollection(features)
                output_file_path = '../ShowPick/public/json/polygons.geojson'
                # Write the GeoJSON feature collection to a file
                with open(output_file_path, 'w') as f:
                    geojson.dump(feature_collection, f, indent=2)

                feature_collection = geojson.FeatureCollection(features_point)

                # Save the GeoJSON feature collection to a file
                with open('../ShowPick/public/json/station.geojson', 'w') as f:
                    geojson.dump(feature_collection, f)
            print(t0,"Update Database",time.time()-a)
        except Exception as e:
            print(e)
            print(a, "Gagal update voronoi cell")
        t0 = datetime.utcnow()