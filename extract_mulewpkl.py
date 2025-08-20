import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns
import glob, pickle, os
import geopandas
from mpl_toolkits.axes_grid1.inset_locator import inset_axes
from PIL import Image
import imageio
from mpl_toolkits.axes_grid1 import make_axes_locatable
from EQdetect.utils.vorstat import voronoi_sta
from EQdetect.utils.config import Config
from EQdetect.utils.report import report
from EQdetect.core.sourcecal import Phase, EQsrc, pending_eq

#65258_20240814091543_2
kk=0
outf = glob.glob("eew_report/4071_20250108*.pkl")
#11647_20241012011228_2.pkl

if len(outf) == 0:
    print(outf)
    raise ValueError("No Report File")

with open("vorcel/250108155947_vorcel.pkl","rb") as file:
    vorcel = pickle.load(file)
with open(outf[-1], "rb") as file:
    data = pickle.load(file)

allsta = []
for sta in data.estgroup:
    df = vorcel.list_station[vorcel.list_station["Kode"] == sta]
    if df.empty:
        continue
    lats = df["Lat"].values[0]
    longs = df["Long"].values[0]
    allsta.append([float(longs), float(lats), sta])  
allsta = np.array(allsta)
maxlon, maxlat = np.max(allsta[:,:2].astype(float), axis=0)
minlon, minlat = np.min(allsta[:,:2].astype(float), axis=0)

station = []
src_eq = []
particles = []
alldata = []
for outfile in outf:
    with open(outfile, "rb") as file:
        data = pickle.load(file)
    alldata.append(data)   
    station.append(data.phase)
    src_eq.append(data.optsrc)
    
    particle = data.particle
    particles.append(particle[particle[:, -1].argsort()])
    
    longmin, latmin = np.min(data.particle[:,:2], axis=0)
    longmax, latmax = np.max(data.particle[:,:2], axis=0)
    if longmax > maxlon:
        maxlon = longmax
    if latmax > maxlat:
        maxlat = latmax
    if longmin < minlon:
        minlon = longmin
    if latmin < minlat:
        minlat = latmin
        
minlon -= 0.5
maxlon += 0.5
minlat -= 0.5
maxlat += 0.5

world = geopandas.read_file(r'C:\Users\wijay\Documents\EEWS_new\ne_10m_land.zip')
world = world.cx[94:120, -10:14]
cmap = sns.color_palette("viridis", as_cmap=True)

dirimage = "eew_report/images/"+outf[0].split("\\")[-1].split("_")[0]
if not os.path.isdir(dirimage):
	os.mkdir(dirimage)
for i in range(len(outf)):
    outfile = outf[i].split("\\")[-1]

    sta = station[i]
    tmpsta = []
    for ss in sta:
        tmpsta.append([ss.longitude, ss.latitude])
        # print(ss.sta)
    tmpsta = np.array(tmpsta)
    
    fig, (ax, ax1) = plt.subplots(2, 1, figsize=(8, 10))

    world.plot(ax=ax, alpha=0.4, color="grey")
    eq = src_eq[i]
    particle = particles[i]
    nsample = len(particle[:, 5])
    allwi = np.full(nsample, 1./ nsample)
    # allwi[np.isnan(allwi)] = 0
    x0 = np.sum(allwi * particle[:, 0])
    y0 = np.sum(allwi * particle[:, 1])
    z0 = np.sum(allwi * particle[:, 2]) 
    print(np.median(allwi))
    
    # id_max = np.argmax(particle[:, 5])
    # [x0, y0, z0, mag0, ot0] = particle[id_max, :5]
    scatter = ax.scatter(particle[:, 0], particle[:, 1],marker='o', c=np.log(particle[:, 5]), cmap=cmap, s=10)
    ax.scatter(allsta[:,0].astype(float), allsta[:,1].astype(float), marker='^', color='white', edgecolors='k', label='Station Group')
    # axins = inset_axes(
    #     ax,
    #     width="5%",  # width: 5% of parent_bbox width
    #     height="100%",  # height: 50%
    #     loc="lower left",
    #     bbox_to_anchor=(1.05, 0., 1, 1),
    #     bbox_transform=ax.transAxes,
    #     borderpad=0,
    # )
    # cbar = fig.colorbar(scatter, cax=axins, pad=0.5)
    # cbar.set_label('Ln Weight')       
    ax.scatter(tmpsta[:,0], tmpsta[:,1], marker='^', color='blue', label='Triggered Station')
    ax.scatter(tmpsta[0,0], tmpsta[0,1], marker='^', color='black', label='First Trigger',s=50)
    ax.scatter(eq[0], eq[1], marker='*', color='red', label='Hypocenter', s=30)
    ax.scatter(x0, y0, marker='*', color='black', label='Hypocenter', s=30)
    ttl = outfile.split("_")
    title = f"{ttl[1]} M:{round(float(eq[3]),2)}"     
    ax.set_title(title)
    ax.set_xlim([minlon, maxlon])
    ax.set_ylim([minlat, maxlat])
    
    # divider = make_axes_locatable(ax)
    # cax = divider.append_axes("right", size="1%", pad=0.1)  # Adjust size and padding
    # cbar = fig.colorbar(scatter, cax=cax)  # Shrink if needed
    # cbar.set_label('Ln Weight')
    # Create the color bar outside of ax
    # Get the position of ax to set the color bar height
    ax_pos = ax.get_position()  # Get the position of ax
    colorbar_height = ax_pos.y1 - ax_pos.y0  # Calculate the height

    # Create the color bar axes with the same height as ax
    cbar_ax = fig.add_axes([0.92, ax_pos.y0, 0.02, colorbar_height])  # Adjust [left, bottom, width, height]
    cbar = fig.colorbar(scatter, cax=cbar_ax)
    cbar.set_label('Ln Weight')
    
    ax1.scatter(particle[:,0], particle[:,2]*-1, marker='o', c=np.log(particle[:, 5]), cmap=cmap, s=10)
    ax1.scatter(eq[0], eq[2]*-1, marker='*', color='red', label='Hypocenter', s=30)
    ax1.scatter(x0, z0*-1, marker='*', color='black', label='Hypocenter', s=30)
    ax1.set_xlim([minlon, maxlon])
    ax1.set_ylim([-150,0])

    # plt.subplots_adjust(left=0.1, right=0.85, top=0.9, bottom=0.1)  # Adjust as needed
    # Memastikan kedua subplot memiliki aspek yang sama
    ax.set_aspect('auto')
    ax1.set_aspect('auto')
    nsample = len(particle[:, 5])
    allwi = np.full(nsample, 1./ nsample)
    x0 = np.nansum(allwi * particle[:, 0])
    y0 = np.nansum(allwi * particle[:, 1])
    z0 = np.nansum(allwi * particle[:, 2])
    varx = np.sqrt(abs(np.nansum((allwi * particle[:, 0]**2)) - x0**2))
    vary = np.sqrt(abs(np.nansum((allwi * particle[:, 1]**2)) - y0**2))
    varz = np.sqrt(abs(np.nansum((allwi * particle[:, 2]**2)) - z0**2))

    print(outfile, eq[:3], varx, vary, varz, alldata[i].need_resample, eq[-1], len(particle), len(tmpsta))
    outfile = f"{dirimage}/{outfile[:-4]}.png"
    plt.savefig(outfile, dpi=300, bbox_inches='tight')
    plt.close()

# Daftar file PNG
image_files = glob.glob(f"{dirimage}/*.png")  # Tambahkan file PNG lainnya

common_size = (800, 600)  # Adjust to your desired dimensions
images = [Image.open(file).convert('RGB').resize(common_size) for file in image_files]

# Simpan animasi sebagai GIF
output_gif = f"{dirimage}/{ttl[0]}.gif"

# Buat animasi dengan durasi 1 detik antar frame
imageio.mimsave(output_gif, images, format='GIF', duration=500)