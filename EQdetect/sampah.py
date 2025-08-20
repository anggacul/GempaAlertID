def arrp(station, config, hyp):
    dis = math.sqrt((station.latitude - hyp.y0) * dlat * (station.latitude - hyp.y0) * dlat +
        (station.longitude - hyp.x0) * dlon * (station.longitude - hyp.x0) * dlon + 0.000000001)
    xc = (dis * dis - 2. * config.v0 / config.vg * hyp.z0 - hyp.z0 * hyp.z0) / (2. * dis)
    zc = -1. * config.v0 / self.vg
    ang1 = math.atan((hyp.z0 - zc) / xc)
    if ang1 < 0.0:
        ang1 = ang1 + math.pi
    ang1 = math.pi - ang1
    ang2 = math.atan(-1. * zc / (dis - xc))
    stime = (-1. / config.vg) * math.log(abs(math.tan(ang2 / 2.) / math.tan(ang1 / 2.)))

    if z0 < self.Boundary_P:
        self.v0 = self.SwP_V
        self.vg = 