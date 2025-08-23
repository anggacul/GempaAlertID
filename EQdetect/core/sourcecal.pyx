import math
import time as ttt
import inspect
import numpy as np
from numpy.random import random
import traceback
from datetime import datetime
cimport numpy as np
from cython.parallel import prange
from libc.stdlib cimport rand, srand
from libc.time cimport time, time_t
from cython cimport boundscheck, wraparound, gil, nogil
import pickle
from libc.math cimport log10, exp, log, pi

with open('interp_func_1.pkl', 'rb') as f:
    interp_func = pickle.load(f)

#Muat data dari file .npy
x_arr = np.load("depths.npy")
y_arr = np.load("distances.npy")
z_arr = np.load("traveltime_grid.npy")


# Disable bounds checking and wraparound for performance
@boundscheck(False)
@wraparound(False)
cdef np.ndarray[double, ndim=2] RPF(np.ndarray[double, ndim=2] particle1, double varx, double vary, double varz, double dmin, double dmax):
    cdef Py_ssize_t N = <int>particle1.shape[0]
    cdef Py_ssize_t i
    cdef double h = (8.0 * 3.0/4.0 * 5.0 * 8.0 * np.sqrt(np.pi) / N)**0.2
    cdef double e0, e1, e2, norm, h_epsilon_varz
    cdef np.ndarray[double, ndim=2] particle = particle1[:, :3]
    cdef np.ndarray[double, ndim=1] e = np.zeros(3, dtype=np.float64)
    cdef np.ndarray[double, ndim=2] x = np.zeros((1, 3), dtype=np.float64)
    cdef np.ndarray[double, ndim=1] epsilon = np.zeros(3, dtype=np.float64)
    
    for i in range(N):
        while True:
            e = np.sqrt(np.random.beta(1.5, 2.0, 3))
            x = np.random.normal(0.0, 1.0, 3).reshape(1, -1)
            norm = np.linalg.norm(x)
            if norm > 0:
                x /= norm
                epsilon = np.multiply(e, x)[0]
                h_epsilon_varz = h * epsilon[2] * varz
                if dmin <= particle[i, 2] + h_epsilon_varz <= dmax:
                    break
        
        particle1[i, 0] = particle[i, 0] + h * epsilon[1] * varx
        particle1[i, 1] = particle[i, 1] + h * epsilon[0] * vary
        particle1[i, 2] = particle[i, 2] + h * epsilon[2] * varz
    
    return particle1

@boundscheck(False)
@wraparound(False)
cdef np.ndarray[double, ndim=2] resample(np.ndarray[double, ndim=1] weights, np.ndarray[double, ndim=2] particles):
    cdef Py_ssize_t N = <int>len(weights)
    cdef np.ndarray[double, ndim=1] Q = np.nancumsum(weights)
    cdef np.ndarray[double, ndim=2] new_particles = np.zeros((N, particles.shape[1]), dtype=np.float64)
    cdef double u0, u,
    cdef Py_ssize_t i, m_old, m = 0
    u0 = 0.0 + 1.0 / N * np.random.uniform(0,1)
    for i in range(N):
        u = u0 + float(i) / N
        m_old = m
        while Q[m] < u:
            m += 1  # no need to reset m, u always increases
        if m>=N:
            m = m_old
        # Add state sample (state)
        try:
            new_particles[i, :] = particles[m, :]
        except Exception as e:
            print(m,i,N,weights.shape[0], len(Q))
            raise ValueError(e)
    
    return new_particles
# Load data as C-contiguous arrays
x_arr = np.ascontiguousarray(np.load("depths.npy"), dtype=np.float64)
y_arr = np.ascontiguousarray(np.load("distances.npy"), dtype=np.float64)
z_arr = np.ascontiguousarray(np.load("traveltime_grid.npy"), dtype=np.float64)

# Get array sizes once
cdef Py_ssize_t x_size = x_arr.shape[0]
cdef Py_ssize_t y_size = y_arr.shape[0]

# Get raw pointers to array data
cdef double[:] x_view = x_arr
cdef double[:] y_view = y_arr
cdef double[:, :] z_view = z_arr

@cython.boundscheck(False)
@cython.wraparound(False)
cpdef double arrp_interp(double x, double y) nogil:
    cdef Py_ssize_t x1, x2, y1, y2
    cdef double x1_val, x2_val, y1_val, y2_val
    cdef double Q11, Q12, Q21, Q22
    cdef double f_xy1, f_xy2, f_xy
    cdef double x_diff, y_diff, x_weight, y_weight
    
    # Find grid indices around the desired point
    x1 = binary_search(x_view, x_size, x) - 1
    x2 = x1 + 1
    
    y1 = binary_search(y_view, y_size, y) - 1
    y2 = y1 + 1
    
    # Check and adjust boundaries
    if x1 < 0:
        x1 = 0
        x2 = 1
    elif x2 >= x_size:
        x2 = x_size - 1
        x1 = x2 - 1
    
    if y1 < 0:
        y1 = 0
        y2 = 1
    elif y2 >= y_size:
        y2 = y_size - 1
        y1 = y2 - 1
    
    # Get values from arrays
    x1_val = x_view[x1]
    x2_val = x_view[x2]
    y1_val = y_view[y1]
    y2_val = y_view[y2]
    
    # Get z values
    Q11 = z_view[x1, y1]
    Q12 = z_view[x1, y2]
    Q21 = z_view[x2, y1]
    Q22 = z_view[x2, y2]
    
    # Calculate weights
    x_diff = x2_val - x1_val
    y_diff = y2_val - y1_val
    
    # Avoid division by zero (if x1_val == x2_val or y1_val == y2_val)
    if x_diff == 0.0:
        x_weight = 0.5
    else:
        x_weight = (x2_val - x) / x_diff
    
    if y_diff == 0.0:
        y_weight = 0.5
    else:
        y_weight = (y2_val - y) / y_diff
    
    # Perform bilinear interpolation
    f_xy1 = x_weight * Q11 + (1.0 - x_weight) * Q21
    f_xy2 = x_weight * Q12 + (1.0 - x_weight) * Q22
    f_xy = y_weight * f_xy1 + (1.0 - y_weight) * f_xy2
    
    return f_xy

# Optimized binary search implementation
cdef inline Py_ssize_t binary_search(double[:] arr, Py_ssize_t size, double value) nogil:
    cdef Py_ssize_t low = 0
    cdef Py_ssize_t high = size
    cdef Py_ssize_t mid
    
    while low < high:
        mid = low + (high - low) // 2
        if arr[mid] < value:
            low = mid + 1
        else:
            high = mid
    return low

@boundscheck(False)
@wraparound(False)
cdef double delaz(double lat1, double lon1, double lat2, double lon2):
    cdef double dlat, dlon
    dlat = (lat2 - lat1) * 111.12
    dlon = (lon2 - lon1) * 111.12 * math.cos(lat1 * np.pi / 180.0)
    return math.sqrt(dlat * dlat + dlon * dlon)

@boundscheck(False)
@wraparound(False)
cdef double phys(double x):
    return math.exp(-0.5 * (x ** 2)) / math.sqrt(2.0 * np.pi)


@boundscheck(False)
@wraparound(False)
cdef tuple gauss_lklhood(double sigmp, double sigma, double pd, double pickerr, double dist, np.ndarray[double, ndim=1] hypo):
    cdef double fp, fa, mua
    fp = -0.5 * (log(sigmp)+((pickerr)**2)/sigmp)
    mua = 10**((hypo[3] - 5.067 - 1.76 *log10(dist))/1.281)
    fa = -0.5 * (log(sigma)+((pd - mua)**2)/sigma) 
    return fp, fa

@boundscheck(False)
@wraparound(False)
cdef double log_avg(np.ndarray[double, ndim=1] weight):
    cdef Py_ssize_t i
    cdef double wi, log_tot
    log_tot = weight[0]

    for i, wi in enumerate(weight):
        if log_tot > wi:
            log_tot = log(exp(wi-log_tot)+1.0) + log_tot
        else:
            log_tot = log(exp(log_tot-wi)+1.0) + wi
    return log_tot

@boundscheck(False)
@wraparound(False)
cdef np.ndarray[double, ndim=2] cal_ipf_edt(np.ndarray[double, ndim=2] particle, list station, list nottrigsta, double timenow):
    cdef Py_ssize_t i, j, k, nsta
    cdef double dist, ptime, perr, fp, fa, mag, ot, weight, delpd, delpd_t
    cdef double fn_all, min_fa, min_fp, fp_all, fa_all, sigmp, sigma, sum_wi

    cdef np.ndarray[double, ndim=1] m = np.zeros(len(station), dtype=np.float64)
    cdef np.ndarray[double, ndim=1] allpt = np.zeros(len(station), dtype=np.float64)
    cdef np.ndarray[double, ndim=1] allpd = np.zeros(len(station), dtype=np.float64)
    cdef np.ndarray[double, ndim=1] allot = np.zeros(len(station), dtype=np.float64)
    cdef np.ndarray[double, ndim=1] alldis = np.zeros(len(station), dtype=np.float64)
    cdef np.ndarray[double, ndim=1] allpo = np.zeros(len(station), dtype=np.float64)
    cdef np.ndarray[double, ndim=1] hypo

    sigmp = 1.5
    sigma = 0.72
    nsta = len(station)
    timenow -= 7*3600
    for k in range(particle.shape[0]):
        hypo = particle[k]

        for i in range(nsta):
            dist = delaz(station[i].latitude, station[i].longitude, hypo[1], hypo[0])
            ptime = arrp_interp(hypo[2], dist / 110.567)
            allpt[i] = ptime
            allpd[i] = station[i].pd
            allot[i] = station[i].picktime - ptime
            allpo[i] = station[i].picktime
            alldis[i] = dist
            m[i] = 5.067 + 1.281 * log10(station[i].pd) + 1.760 * log10(dist)

        if len(m) == 1:
            mag = m[0]
            ot = allot[0]
        else:
            mag = round(np.median(m), 2)
            ot = round(np.median(allot), 2)

        hypo[3] = mag
        hypo[4] = ot

        fn_all = 0.0
        min_fa = 0.0
        min_fp = 9999.9

        fp_all = 0.0
        fa_all = 0.0
        if nsta == 1:
            for i in range(nsta):
                perr = allpo[i] - (ot + allpt[i])
                fp = -0.5 * (log(sigmp)+((perr)**2)/sigmp)
                fp_all += fp
        else:
            for i in prange(nsta, nogil=True):
                for j in range(i + 1, nsta):
                    perr = (allpo[i] - allpo[j]) - (allpt[i] - allpt[j])
                    delpd = log10(allpd[i] / allpd[j])
                    delpd_t = 1.374 * log10(alldis[j] / alldis[i])
                    fp = -0.5 * (log(sigmp)+((perr)**2)/sigmp)
                    fa = -0.5 * (log(sigma)+((delpd - delpd_t)**2)/sigma) 
                    fp_all += fp
                    fa_all += fa

        for sta in nottrigsta:
            dist = delaz(sta[2], sta[1], hypo[1], hypo[0])
            ptime = arrp_interp(hypo[2], dist / 110.567) if round(dist, 3) != 0.0 else 0.0
            perr = (ptime - allpt[0]) - (timenow - allpo[0])
            fp = 0 if perr >= 0 else gauss_lklhood(1.5, 0.07, 1.0, perr, dist, hypo)[0]
            if fp < -10.0:
                fp = -10.0
            fn_all += fp

        weight = fp_all + fa_all + fn_all
        hypo[5] = weight
        hypo[6] = fn_all
        hypo[7] = fa_all
        particle[k] = hypo

    sum_wi = log_avg(particle[:, 5])
    particle[:, 5] = np.exp(particle[:, 5] - sum_wi)
    particle[:, 5] = np.nan_to_num(particle[:, 5])
    return particle
# Disable bounds checking and wraparound for performance
@boundscheck(False)
@wraparound(False)
cdef np.ndarray[double, ndim=2] gridhypo(np.ndarray[double, ndim=1] point, double dx, double dy, Py_ssize_t num_n=16):
    cdef double x_min, x_max, y_min, y_max, z_min, z_max
    cdef Py_ssize_t i, j, k
    cdef np.ndarray[double, ndim=1] z_values
    cdef np.ndarray[double, ndim=1] x_values, y_values, z_values1
    cdef np.ndarray[double, ndim=3] x_grid, y_grid, z_grid
    cdef np.ndarray[double, ndim=2] point_grd
    cdef Py_ssize_t total_points = num_n * num_n * num_n
    
    x_min, x_max = point[0] - dx, point[0] + dx
    y_min, y_max = point[1] - dy, point[1] + dy
    z_min, z_max = 0.0, 100.0
    
    # Initialize the random number generator
    #srand(time(NULL))
    cdef time_t current_time = time(NULL)
    cdef unsigned int seed = <unsigned int>current_time  # Cast ke unsigned int
    srand(seed)    
    z_values = np.random.uniform(z_min, z_max, total_points)
    x_values = np.arange(x_min, x_max, dx*2/num_n)
    y_values = np.arange(y_min, y_max, dy*2/num_n)
    z_values1 = np.arange(z_min, z_max, z_max/num_n)
    
    x_grid, y_grid, z_grid = np.meshgrid(x_values, y_values, z_values1, indexing='ij')
    
    point_grd = np.zeros((total_points, 8), dtype=np.float64)
    
    cdef Py_ssize_t index = 0
    for i in range(num_n):
        for j in range(num_n):
            for k in range(num_n):
                point_grd[index, 0] = round(x_grid[i, j, k], 5)
                point_grd[index, 1] = round(y_grid[i, j, k], 5)
                point_grd[index, 2] = z_values[index]
                index += 1
    
    return point_grd
    
def pending_eq(eqdata, vorcel):
    eqdata.cal_hypo(vorcel)
    if eqdata.optsrc[3] > 10.0 or eqdata.optsrc[3] <= 0.0 or abs(eqdata.rmse)<0.001:
        return False, eqdata
    eqdata.endtime = datetime.utcnow()
    eqdata.update = eqdata.update + 1
    print(
        f"Pending Earthquake {eqdata.first.sta} No {eqdata.noeq} with paramater {eqdata.starttime}\n")
    file = open("output.txt", "a")
    file.write(
        f"Pending Earthquake No {eqdata.noeq} with paramater {eqdata.starttime}\n")
    file.write(
        f"Longitude : {round(eqdata.optsrc[0],3)}+-{round(eqdata.optsrc[5],3)}\nLatitude : {round(eqdata.optsrc[1],3)}+-{round(eqdata.optsrc[6],3)}\nDepth : {round(eqdata.optsrc[2],5)}\nOT : {round(eqdata.optsrc[4],2)}\nMag : {round(eqdata.optsrc[3],2)}\n")
    file.write(
        f"First Trigger : {eqdata.first.sta} {eqdata.triggroup}\n")
    file.write(
        f"First Trigger prior : {eqdata.first.weight} {eqdata.rmse}\n\n")
    file.close()
    return True, eqdata

class Phase:
    def __init__(self, pick, vorcel):
        #pick = pick.split(" ")
        if len(pick) < 13:
            raise ValueError("Length of pick message is not appropiate")
        self.sta = pick[0]
        self.comp = pick[1]
        self.pa = float(pick[6])
        self.pv = float(pick[7])
        self.pd = float(pick[8])
        self.picktime = float(pick[10])
        self.weight = float(pick[11])
        self.telflag = float(pick[12])
        self.upd_sec = float(pick[13])
        self.reserr = 999
        self.status = 0
        df = vorcel.list_station[vorcel.list_station["Kode"] == self.sta]
        self.longitude = float(df["Long"].values[0])
        self.latitude = float(df["Lat"].values[0])

    def set_first(self, voronoi):
        #if inspect.isclass(voronoi) is False:
        #    raise ValueError("The voronoi input is not Class")
        #try:
        #    idx = voronoi.code.index(self.sta)
        #except:
        #    print(f"{self.sta} is not in list station database")
        #    return False
        #self.triggroup = voronoi.sta_trgg[idx]
        #self.estgroup = voronoi.sta_estg[idx]
        #self.init_eq = voronoi.init_eq[idx]
        #return True
        try:
            self.triggroup, self.estgroup, self.init_eq = voronoi.get_vor(self.sta)
            return True
        except:
            print(f"{self.sta} is not in list station database")
            return False
    
    def calerror(self, hypo):
        dist = delaz(self.latitude, self.longitude, hypo[1], hypo[0])
        ptime = arrp_interp(hypo[2], dist/110.567)
        self.reserr = self.picktime - hypo[4] - ptime

cdef class EQsrc:
    """
    status = 0 is pending earthquake
    status = -1 is cancel earthquake
    status = 1 is ongoing earthquake 
    status = -3 is change first pick
    """

    cdef public object first
    cdef public Py_ssize_t status
    cdef public list initsrc
    cdef public list triggroup
    cdef public list estgroup
    cdef public object starttime
    cdef public object endtime
    cdef public list rmphase
    cdef public list phase
    cdef public list code
    cdef public Py_ssize_t numtrg
    cdef public list optsrc
    cdef public Py_ssize_t update
    cdef public object cfg
    cdef public list var
    cdef public bint report
    cdef public double old_nsta
    cdef public Py_ssize_t noeq
    cdef public bint need_resample
    cdef public double rmse
    cdef public double optneff
    cdef public object particle

    def __init__(self, ftrig, cfg, Py_ssize_t noeq):
        self.first = ftrig
        self.status = 0
        self.initsrc = ftrig.init_eq
        self.triggroup = ftrig.triggroup
        self.estgroup = ftrig.estgroup
        self.starttime = datetime.utcnow()
        self.endtime = datetime.utcnow()
        self.rmphase = []
        self.phase = [ftrig]
        self.code = [ftrig.sta]
        self.numtrg = 0
        self.optsrc = [self.initsrc[0], self.initsrc[1], 10.0, 0.0, 0.0, 0.0]
        self.update = 0
        self.cfg = cfg
        self.var = []
        self.report = False
        self.noeq = noeq
        self.need_resample = False
        self.rmse = 0.0
        self.particle = None
        self.optneff = 9999
        self.old_nsta = len(self.phase)

    cpdef bint check_trgg(self, pick):
        cdef Py_ssize_t idx
        cdef object picking

        pick.calerror(self.optsrc)
        if abs(pick.reserr) <= self.cfg.trig_err and pick.sta in self.triggroup:
            if pick.sta in self.code:
                idx = self.code.index(pick.sta)
                picking = self.phase[idx]
                if picking.picktime == pick.picktime:
                    self.phase[idx] = pick
                else:
                    return False
            else:
                self.phase.append(pick)
                self.code.append(pick.sta)
                self.numtrg += 1
        elif abs(pick.reserr) <= self.cfg.est_trig_err and pick.sta in self.estgroup:
            if pick.sta in self.code:
                idx = self.code.index(pick.sta)
                picking = self.phase[idx]
                if picking.picktime == pick.picktime:
                    self.phase[idx] = pick
                else:
                    return False
            else:
                self.phase.append(pick)
                self.code.append(pick.sta)
        else:
            return False
        return True

    cpdef bint check_estg(self, pick):
        cdef Py_ssize_t idx
        cdef object picking

        if pick.sta in self.estgroup:
            if pick.sta in self.code:
                idx = self.code.index(pick.sta)
                picking = self.phase[idx]
                if picking.picktime == pick.picktime:
                    self.phase[idx] = pick
                else:
                    return False
            else:
                pick.calerror(self.optsrc)
                if abs(pick.reserr) <= self.cfg.trig_err and self.update < 1:
                    self.phase.append(pick)
                    self.code.append(pick.sta)
                elif abs(pick.reserr) <= self.cfg.est_trig_err and self.update > 1:
                    self.phase.append(pick)
                    self.code.append(pick.sta)
                else:
                    return False
        else:
            pick.calerror(self.optsrc)
            if abs(pick.reserr) <= self.cfg.est_trig_err:
                return True
            if abs(pick.reserr) <= self.cfg.est_trig_err + 1.5 and self.update > 6:
                return True
            return False
        return True

    cpdef void eq_stat(self, vorcel):
        cdef Py_ssize_t i, nsta, fp_rm
        cdef double dttime

        if self.numtrg >= 2 and self.status == 0:
            self.status = 1
        if self.numtrg >= 1 and self.status == 0 and len(self.phase) >= 4:
            self.status = 1
        dttime = (datetime.utcnow() - self.starttime).total_seconds()
        if self.status == 0:
            if dttime >= 80:
                self.status = -1
        if self.status >= 1:
            if (self.optsrc[3] >= 6 and dttime > 8 * 60) or (self.optsrc[3] < 6 and dttime > 5 * 60):
                self.status = -1
        if self.status == 1:
            if self.update >= 50:
                self.status = 2
                return
            fp_rm = 0
            nsta = len(self.phase)
            i = 0
            while i < nsta:
                if i > len(self.phase) - 1:
                    break
                self.phase[i].calerror(self.optsrc)
                if abs(self.phase[i].reserr) > self.cfg.est_trig_err:
                    self.rmphase.append(self.phase[i])
                    if self.phase[i].sta == self.first.sta:
                        self.phase[i].status = -1
                        print(f"Remove first pick in estimation with code {self.phase[i].sta} {self.phase[i].reserr}")
                        fp_rm = 1
                    else:
                        print(f"Remove pick in estimation with code {self.phase[i].sta} {self.phase[i].reserr}")
                    if self.phase[i].sta in self.code:
                        idx = self.code.index(self.phase[i].sta)
                        self.code.pop(idx)
                    self.phase.pop(i)
                    if len(self.phase) == 0 or i == nsta - 1:
                        i = nsta + 1
                        continue
                else:
                    i += 1
                    if i == nsta - 1:
                        i = nsta + 1
            if len(self.phase) < 2:
                self.status = -1
                for pick in self.phase:
                    self.rmphase.append(pick)
                return
            else:
                if fp_rm == 1:
                    timepick = 99999999999999999
                    for pick in self.phase:
                        if pick.picktime < timepick:
                            self.first = pick
                            timepick = pick.picktime
                    self.first.set_first(vorcel)
                    self.initsrc = self.first.init_eq
                    self.triggroup = self.first.triggroup
                    self.estgroup = self.first.estgroup
                    self.numtrg = len(self.phase)
                    self.optsrc = [self.initsrc[0], self.initsrc[1], 10.0, 0.0, 0.0, 0.0]
                    self.status = 1
                    return
            if self.update > 10 and len(self.phase) < 3:
                self.status = -1
                for pick in self.phase:
                    if pick.sta != self.first.sta:
                        self.rmphase.append(pick)

    cpdef void cal_hypo(self, vorcel):
        cdef Py_ssize_t i, len_estgroup, len_code, len_phase, len_nottrigsta, nsample
        cdef double x0, y0, z0, mag0, ot0, varx0, vary0, varz0, varot0, dmin, dmax, neff, dist, ptime, a
        cdef object pick, df
        cdef np.ndarray[np.float64_t, ndim=2] particle_tmp
        cdef np.ndarray[np.float64_t, ndim=1] allwi
        cdef list nottrigsta = []
        cdef list allot = []
        cdef list magall = []

        if self.update <= 1:
            self.optsrc[0] = self.initsrc[0]
            self.optsrc[1] = self.initsrc[1]
        eq_cent = np.array([self.optsrc[0], self.optsrc[1]], dtype=np.float64)

        if not self.need_resample:
            particle_tmp = gridhypo(eq_cent, self.initsrc[2], self.initsrc[3], 13)
        else:
            particle_tmp = self.particle

        nsample = len(particle_tmp)

        # Find not triggered stations
        len_estgroup = len(self.estgroup)
        len_code = len(self.code)
        for i in range(len_estgroup):
            if self.estgroup[i] in self.code:
                continue
            df = vorcel.list_station[vorcel.list_station["Kode"] == self.estgroup[i]]
            if df.empty:
                continue
            lats = df["Lat"].values[0]
            longs = df["Long"].values[0]
            nottrigsta.append([self.estgroup[i], longs, lats])

        a = ttt.time()
        particle_tmp = cal_ipf_edt(particle_tmp, self.phase, nottrigsta, self.endtime.timestamp())
        allwi = particle_tmp[:, 5]
        neff = 1. / np.nansum(np.square(allwi))
        allwi = np.full(nsample, 1./ nsample)

        x0 = np.nansum(allwi * particle_tmp[:, 0])
        y0 = np.nansum(allwi * particle_tmp[:, 1])
        z0 = np.nansum(allwi * particle_tmp[:, 2])
        varx0 = np.sqrt(abs(np.nansum((allwi * particle_tmp[:, 0]**2)) - x0**2))
        vary0 = np.sqrt(abs(np.nansum((allwi * particle_tmp[:, 1]**2)) - y0**2))
        varz0 = np.sqrt(abs(np.nansum((allwi * particle_tmp[:, 2]**2)) - z0**2))
        dmin = np.min(particle_tmp[:, 2])
        dmax = np.max(particle_tmp[:, 2])

        # Resampling
        if self.update > 0 and neff < len(allwi) / 2:
            #if (neff <= self.optneff and len(self.phase)==self.old_nsta) or len(self.phase)!=self.old_nsta:
            if neff >= 1:
                try:
                    allwi = particle_tmp[:, 5]
                    print("Update Resample", neff)
                    particle_tmp = resample(allwi, particle_tmp)
                    particle_tmp = RPF(particle_tmp, varx0, vary0, varz0, dmin, dmax)
                    particle_tmp = cal_ipf_edt(particle_tmp, self.phase, nottrigsta, self.endtime.timestamp())
                    self.optneff = neff + 0.1*neff
                    self.old_nsta = len(self.phase)
                    self.need_resample = True
                except Exception as e:
                    print(e, str(traceback.extract_stack()[-1][1]))
                    print("RPF Error", neff)

        allwi = np.full(nsample, 1./ nsample)
        x0 = np.nansum(allwi * particle_tmp[:, 0])
        y0 = np.nansum(allwi * particle_tmp[:, 1])
        z0 = np.nansum(allwi * particle_tmp[:, 2])
        varx0 = np.sqrt(abs(np.nansum((allwi * particle_tmp[:, 0]**2)) - x0**2))
        vary0 = np.sqrt(abs(np.nansum((allwi * particle_tmp[:, 1]**2)) - y0**2))
        varz0 = np.sqrt(abs(np.nansum((allwi * particle_tmp[:, 2]**2)) - z0**2))
        
        allwi = particle_tmp[:, 5]
        id_max = np.argmax(particle_tmp[:, 5])
        if self.update <= 1:
            x0 = np.nansum(allwi * particle_tmp[:, 0])
            y0 = np.nansum(allwi * particle_tmp[:, 1])
            z0 = np.nansum(allwi * particle_tmp[:, 2])
        else:
            [x0, y0, z0, mag0, ot0] = particle_tmp[id_max, :5]
            if self.need_resample:
                allwi = np.full(nsample, 1./ nsample)
                x0 = np.nansum(allwi * particle_tmp[:, 0])
                y0 = np.nansum(allwi * particle_tmp[:, 1])
                z0 = np.nansum(allwi * particle_tmp[:, 2])

        if self.update == 0:
            z0 = 10.0

        for pick in self.phase:
            dist = delaz(pick.latitude, pick.longitude, y0, x0)
            magall.append(5.067 + 1.281 * np.log10(pick.pd) + 1.760 * np.log10(dist))
            ptime = arrp_interp(z0, dist / 110.567)
            allot.append(pick.picktime - ptime)
        ot0 = round(np.median(allot), 2)
        mag0 = np.median(magall)
        varot0 = np.sqrt(abs(np.nansum((allwi * particle_tmp[:, 4]**2)) - ot0**2))
        
        cdef np.ndarray[np.float64_t, ndim=1] rmse = np.zeros(len(self.phase), dtype=np.float64)
        for i in range(len(self.phase)):
            self.phase[i].calerror([x0, y0, z0, mag0, ot0, varx0, vary0, varz0, neff])
            rmse[i] = self.phase[i].reserr
        self.rmse = np.sqrt(np.mean(rmse**2))

        if delaz(self.optsrc[1], self.optsrc[0], y0, x0) > 100 or len(self.phase) < 3 or varx0 > 0.15 or vary0 > 0.15:
            self.need_resample = False

        a = ttt.time() - a
        self.optsrc = [x0, y0, z0, mag0, ot0, varx0, vary0, varz0, neff]
        self.first.weight = a
        self.particle = particle_tmp
        self.var.append([x0, y0])