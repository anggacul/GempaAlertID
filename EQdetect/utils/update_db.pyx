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
    mycursor.execute(f"SELECT Kode, Status_tele FROM tb_avaccelro where Tipe LIKE '%Accelerograph%'")
    station = mycursor.fetchall()
    mydb.commit()
    mydb.close()

    mydb = mysql.connector.connect(**config)
    mycursor = mydb.cursor()
    for sta in station:
        if sta[1]=="OFF":
            sql = f"UPDATE meta_playback SET status=0 WHERE Kode='{sta[0]}'"
        else:
            sql = f"UPDATE meta_playback SET status=1 WHERE Kode='{sta[0]}'"
        mycursor.execute(sql)
        mydb.commit()
    mydb.close()
