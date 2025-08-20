import mysql.connector
import numpy as np
from scipy.spatial import Voronoi, KDTree
import matplotlib.pyplot as plt
import collections.abc
from obspy.geodetics import gps2dist_azimuth, degrees2kilometers
from geopy import Point
from geopy.distance import geodesic
from ast import literal_eval
import geopandas
import pandas as pd

def voronoi_finite_polygons_2d(vor, radius=None):
    """
    Reconstruct infinite voronoi regions in a 2D diagram to finite
    regions.

    Parameters
    ----------
    vor : Voronoi
        Input diagram
    radius : float, optional
        Distance to 'points at infinity'.

    Returns
    -------
    regions : list of tuples
        Indices of vertices in each revised Voronoi regions.
    vertices : list of tuples
        Coordinates for revised Voronoi vertices. Same as coordinates
        of input vertices, with 'points at infinity' appended to the
        end.

    """

    if vor.points.shape[1] != 2:
        raise ValueError("Requires 2D input")

    new_regions = []
    new_vertices = vor.vertices.tolist()

    center = vor.points.mean(axis=0)
    if radius is None:
        radius = vor.points.ptp().max()
        #(radius)

    # Construct a map containing all ridges for a given point
    all_ridges = {}
    for (p1, p2), (v1, v2) in zip(vor.ridge_points, vor.ridge_vertices):
        all_ridges.setdefault(p1, []).append((p2, v1, v2))
        all_ridges.setdefault(p2, []).append((p1, v1, v2))

    # Reconstruct infinite regions
    for p1, region in enumerate(vor.point_region):
        vertices = vor.regions[region]

        if all(v >= 0 for v in vertices):
           # finite region
            new_regions.append(vertices)
            continue

        # reconstruct a non-finite region
        ridges = all_ridges[p1]
        new_region = [v for v in vertices if v >= 0]

        for p2, v1, v2 in ridges:
            if v2 < 0:
                v1, v2 = v2, v1
            if v1 >= 0:
                # finite ridge: already in the region
                continue

            # Compute the missing endpoint of an infinite ridge

            t = vor.points[p2] - vor.points[p1]  # tangent
            t /= np.linalg.norm(t)
            n = np.array([-t[1], t[0]])  # normal

            midpoint = vor.points[[p1, p2]].mean(axis=0)
            direction = np.sign(np.dot(midpoint - center, n)) * n
            far_point = vor.vertices[v2] + direction * radius

            new_region.append(len(new_vertices))
            new_vertices.append(far_point.tolist())

        # sort region counterclockwise
        vs = np.asarray([new_vertices[v] for v in new_region])
        c = vs.mean(axis=0)
        angles = np.arctan2(vs[:, 1] - c[1], vs[:, 0] - c[0])
        new_region = np.array(new_region)[np.argsort(angles)]

        # finish
        new_regions.append(new_region.tolist())

    return new_regions, np.asarray(new_vertices)


class voronoi_sta:
    """
    The function to calculate voronoi cell from station network distribution
    the ouput from this function can be used for etstimate trigger group and estimation group

    """

    def __init__(self, config=None):
        if config is None:
            self.config = {
                'host': 'localhost',
                'user':'root',
                'database': 'ipfeew'
            }
        else:
            self.config = config
        self.regions = []
        self.vertices = []
        self.vorn = []
        self.cen_p = []
        self.triggroup = []
        self.estgroup = []
        self.init = []
        self.init_eq=[]
        self.sta_trgg = []
        self.sta_estg = []
        mydb = mysql.connector.connect(**self.config)
        mycursor = mydb.cursor()
        mycursor.execute(f"SELECT Kode, Latitude, Longitude FROM meta_playback")
        station = mycursor.fetchall()
        mydb.commit()
        self.list_station = pd.DataFrame(station, columns=["Kode", "Lat", "Long"])

    def find_neighb(self, num_n, point, dist=None, on_points=False):
        if not self.points:
            raise ValueError("There is No point from network")
        tree = KDTree(self.points)
        if dist is None:
            distances, indices = tree.query(point, k=num_n)
        else:
            indices = tree.query_ball_point(point, dist)
        if on_points is True:
            if isinstance(indices, (collections.abc.Sequence, np.ndarray)):
                neighbors = indices[1:]  # lewati diri sendiri
            else:
                neighbors = indices
        else:
            neighbors = indices
        return neighbors

    def find_center(self, region):
        polygon = self.vertices[region]
        center = np.mean(polygon, axis=0)
        return center

    def find_estgroup(self, idx_trig, center, point, id_sta):
        # find 20 closest stations from first station
        id_fst = self.find_neighb(20, point, on_points=True)
        # find 20 closest stations from centre of voronoi cell
        id_cntr = self.find_neighb(30, center)
        # find station within 200km from first station
        id_cls = self.find_neighb(5, point, dist=2, on_points=True)

        for i in id_fst:
            if i in idx_trig:
                continue
            else:
                if i in id_cntr:
                    idx_trig = np.append(idx_trig, i)
        for i in id_cntr:
            if i in idx_trig or i == id_sta:
                continue
            else:
                if len(idx_trig) > 29:
                    break
                idx_trig = np.append(idx_trig, i)
        if isinstance(id_cls, (collections.abc.Sequence, np.ndarray)):
            distances = [np.linalg.norm(np.array(self.points[i]) - np.array(self.points[id_sta])) for i in id_cls]
            id_cls1 = [i for _, i in sorted(zip(distances, id_cls))]
            for i in id_cls1:
                if len(idx_trig) > 29:

                    break
                else:
                    if i in idx_trig:
                        continue
                    else:
                        idx_trig = np.append(idx_trig, i)
        return idx_trig

    def cal_vor(self, station):
        self.points = []
        self.code = []
        for sta in station:
            self.points.append([sta[2], sta[1]])
            self.code.append(sta[0])
        self.vorn = Voronoi(self.points)
        regions, vertices = voronoi_finite_polygons_2d(self.vorn, 10)
        self.regions = regions
        self.vertices = vertices

    def init_area(self, center, sta, triggroup):
        #triggroup = triggroup.tolist()
        # find initial epicenter from voronoi cell
        #origin = Point(float(sta[1]), float(sta[2]))
        dist, az1, az2 = gps2dist_azimuth(
            center[1], center[0], float(sta[1]), float(sta[2]))
        dist = dist/1000
        #if dist >= 50:
        #    destination = geodesic(kilometers=30).destination(
        #        point=origin, bearing=az2)
        #    center = [destination.longitude, destination.latitude]
        #triggroup.append([float(sta[2]), float(sta[1])])
        #triggroup.append(center)
        #triggroup = np.array(triggroup)
        maxp = np.max(triggroup, axis=0)
        minp = np.min(triggroup, axis=0)
        if dist >= 100:
            # right side
            if az2 > 45 and az2 <= 135:
                maxx, minx = maxp[0] + 0.8, minp[0] - 0.01
                maxy, miny = maxp[1] + 0.01, minp[1] - 0.01
            # below
            elif az2 > 135 and az2 <= 225:
                maxx, minx = maxp[0] + 0.01, minp[0] - 0.01
                maxy, miny = maxp[1] + 0.01, minp[1] - 0.8
            # left side
            elif az2 > 225 and az2 <= 315:
                maxx, minx = maxp[0] + 0.01, minp[0] - 0.8
                maxy, miny = maxp[1] + 0.01, minp[1] - 0.01
            # upside
            else:
                maxx, minx = maxp[0] + 0.01, minp[0] - 0.01
                maxy, miny = maxp[1] + 0.8, minp[1] - 0.01
        else:
            maxx, minx = maxp[0], minp[0]
            maxy, miny = maxp[1], minp[1]
        if abs(maxx-minx)<=1.0:
            diff = (1-abs(maxx-minx))/2
            maxx += diff
            minx -= diff
        #if abs(maxx-minx)>=3.5:
        #    diff = (abs(maxx-minx)-2.0)/2
        #    maxx -= diff
        #    minx += diff
        if abs(maxy-miny)<=1.0:
            diff = (1-abs(maxy-miny))/2
            maxy += diff
            miny -= diff
        #if abs(maxy-miny)>=3.5:
        #    diff = (abs(maxy-miny)-2.0)/2
        #    maxy -= diff
        #    miny += diff
        return maxx, minx, maxy, miny

    def plot_vor(self):
        if self.vorn:
            for i, region in enumerate(self.regions):
                polygon = self.vertices[region]
                plt.fill(*zip(*polygon), alpha=0.4)
                center = np.mean(polygon, axis=0)
                plt.plot(center[0], center[1], 'rx')
                # plt.text(center[0],center[1], f"c {self.code[i]}",ha='center')
            for i, p in enumerate(self.points):
                plt.plot(p[0], p[1], 'ko')
                plt.text(p[0], p[1], self.code[i], ha='center')
            plt.show()
        else:
            raise ValueError("Calculate voronoi cell first")

    def plot_vor_sta(self, kode):
        idx = self.code.index(kode)
        point_sta = self.points[idx]
        point_trig = self.triggroup[idx]
        point_est = self.estgroup[idx]
        init_eq = self.init[idx]
        point_cent = self.cen_p[idx]
        fig, ax = plt.subplots(figsize=(8,12))
        world = geopandas.read_file(r'C:\Users\wijay\Documents\EEWS_new\ne_10m_land.zip')
        world = world.cx[94:120, -10:14]
        world.plot(ax=ax, alpha=0.4, color="grey")
        for i, region in enumerate(self.regions):
            polygon = self.vertices[region]
            ax.plot(*zip(*polygon), 'k', linewidth=0.5)
        for i, p in enumerate(self.points):
            ax.plot(p[0], p[1], 'k^')
        for i, p in enumerate(point_est):
            ax.plot(p[0], p[1], 'b^')
        for i, p in enumerate(point_trig):
            ax.plot(p[0], p[1], 'r^')
        ax.plot(point_cent[0], point_cent[1], 'r*', linewidth=3.0)
        ax.plot(point_sta[0], point_sta[1], 'g^', linewidth=3.0)
        ax.plot(*zip(*init_eq), 'r', linewidth=0.5)
        ax.set_xlim([85, 150])
        ax.set_ylim([-15, 14])
        plt.show()

    def get_vor(self, code):
        mydb = mysql.connector.connect(**self.config)
        mycursor = mydb.cursor()
        mycursor.execute(f"SELECT * FROM meta_playback WHERE Kode = '{code}' and Status = 1")
        station = mycursor.fetchall()
        mydb.commit()
        if len(station)==0:
            raise ValueError(
                "Station is not in the database")
        sta_trgg = literal_eval(station[0][4])
        sta_estg = literal_eval(station[0][5])
        loc_group = []
        for sta in sta_estg:
            df = self.list_station[self.list_station["Kode"] == sta]
            loc_group.append([df["Lat"].values[0], df["Long"].values[0]])
        init_eq = [float(numeric_string) for numeric_string in literal_eval(station[0][6])]
        return sta_trgg, sta_estg, init_eq

    def update_trig(self):
        mydb = mysql.connector.connect(**self.config)
        mycursor = mydb.cursor()
        mycursor.execute(f"SELECT * FROM meta_playback WHERE Status = 1")
        station = mycursor.fetchall()
        mydb.commit()
        self.cal_vor(station)
        if len(station) != len(self.regions):
            raise ValueError(
                "Length Station is not same with length Voronoi Region")
        rowupdate = 0
        for i, sta in enumerate(station):
            #if sta[0][:3] == "Vir":
            #    continue
            center = self.find_center(self.regions[i])
            self.cen_p.append(center)

            idx_trig = self.find_neighb(5, self.points[i], on_points=True)
            trig_group = np.array(self.code)[idx_trig]
            sta_triggrp = np.array(self.points)[idx_trig]
            self.triggroup.append(sta_triggrp)
            self.sta_trgg.append(trig_group)

            idx_estgp = self.find_estgroup(idx_trig, center, self.points[i], i)
            est_group = np.array(self.code)[idx_estgp]
            sta_estgrp = np.array(self.points)[idx_estgp]
            self.estgroup.append(sta_estgrp)
            self.sta_estg.append(est_group)
            
            #find earthquake initial area
            #print(sta_triggrp,self.vertices[self.regions[i]])
            dist, az1, az2 = gps2dist_azimuth(
                center[1], center[0], float(sta[1]), float(sta[2]))
            dist = dist/1000
            if dist > 10:
                maxx, minx, maxy, miny = self.init_area(center, sta, self.vertices[self.regions[i]])
            else:
                maxx, minx, maxy, miny = self.init_area(center, sta, sta_triggrp)
            # if sta[0] == "SPSM":
            #    print(center, maxx, minx, maxy, miny, self.vertices[self.regions[i]], sta_triggrp)
            #except Exception as e:
            #print(self.vertices[self.regions[i]],self.regions[i],sta,self.vorn.points.ptp().max())
            #raise TypeError(e)
            eq_init = [[maxx, maxy], [maxx, miny], [minx, miny], [minx, maxy], [maxx, maxy]] 
            init_dx = (maxx-minx)/2
            init_dy = (maxy-miny)/2
            init_cent = [(maxx+minx)/2, (maxy+miny)/2] 
            center = [str(round(center[0], 5)), str(round(center[1], 5))]
            self.init.append(eq_init)
            self.init_eq.append([round(init_cent[0],5), round(init_cent[1],5), round(init_dx,5), round(init_dy,5)])
            sql = f"""UPDATE meta_playback Set cent_vor = "{center}", triggroup = "{trig_group.tolist()}", estgroup = "{est_group.tolist()}", init = "{[str(round(init_cent[0],5)), str(round(init_cent[1],5)), str(round(init_dx,5)), str(round(init_dy,5))]}" WHERE Kode = '{sta[0]}'"""
            mycursor.execute(sql)
            mydb.commit()
            rowupdate += mycursor.rowcount
        if rowupdate <= 0:
            print("No Update Data")
        return rowupdate
    def insert_sta(self, station):
        mydb = mysql.connector.connect(**self.config)
        mycursor = mydb.cursor()
        for i, sta in enumerate(station):
            #mycursor.execute(f"SELECT * FROM meta_playback WHERE Kode = '{sta[0]}'")
            #data = mycursor.fetchall()
            #if data:
            #    continue
            if len(sta)==4:
                sql = f"INSERT INTO meta_playback (Kode, Latitude, Longitude, status) VALUES ('{sta[0]}', '{float(sta[1])}', '{float(sta[2])}', '{int(sta[3])}') ON DUPLICATE KEY UPDATE status='{int(sta[3])}'"            
            else:
                sql = f"INSERT INTO meta_playback (Kode, Latitude, Longitude, status) VALUES ('{sta[0]}', '{float(sta[1])}', '{float(sta[2])}', '{int(sta[6])}') ON DUPLICATE KEY UPDATE status='{int(sta[6])}'"
            mycursor.execute(sql)
            mydb.commit()
