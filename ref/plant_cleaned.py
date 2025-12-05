import numpy as np
from stl import mesh
import math

def generate_implant_stl(filename="implant.stl"):
    # --- \uc0\u21442 \u25968 \u35774 \u32622  (\u21333 \u20301 : mm) ---
    # \uc0\u24635 \u20307 \u23610 \u23544 
    total_length = 13.0      # \uc0\u31181 \u26893 \u20307 \u24635 \u38271 \u24230 
    body_radius = 2.0        # \uc0\u20027 \u20307 \u21322 \u24452  (\u30452 \u24452  4mm)
    collar_height = 2.5      # \uc0\u39030 \u37096 \u20809 \u28369 \u39048 \u22280 \u30340 \u39640 \u24230 
    collar_top_radius = 2.4  # \uc0\u39030 \u37096 \u24179 \u21488 \u21322 \u24452  (\u31245 \u24494 \u22806 \u25193 \u65292 \u24418 \u25104 \u38181 \u24230 )
    
    # \uc0\u24213 \u37096 \u35774 \u35745 
    apex_length = 2.0        # \uc0\u24213 \u37096 \u23376 \u24377 \u22836 /\u22278 \u39030 \u30340 \u38271 \u24230 
    
    # \uc0\u34746 \u32441 \u21442 \u25968 
    thread_pitch = 1.2       # \uc0\u34746 \u36317  (\u27599 \u22280 \u30340 \u22402 \u30452 \u36317 \u31163 )
    thread_depth = 0.25      # \uc0\u34746 \u32441 \u28145 \u24230 
    thread_start_z = apex_length + 0.5  # \uc0\u34746 \u32441 \u36215 \u22987 \u39640 \u24230 
    thread_end_z = total_length - collar_height - 0.2 # \uc0\u34746 \u32441 \u32467 \u26463 \u39640 \u24230 
    
    # \uc0\u20998 \u36776 \u29575  (\u36234 \u39640 \u36234 \u24179 \u28369 \u65292 \u20294 \u25991 \u20214 \u36234 \u22823 )
    res_angular = 80         # \uc0\u22278 \u21608 \u26041 \u21521 \u30340 \u20998 \u27573 \u25968 
    res_vertical = 300       # \uc0\u22402 \u30452 \u26041 \u21521 \u30340 \u20998 \u27573 \u25968 
    
    # --- \uc0\u29983 \u25104 \u32593 \u26684 \u28857  ---
    
    # \uc0\u21019 \u24314  Z \u36724  (\u39640 \u24230 ) \u21644  Theta (\u35282 \u24230 ) \u30340 \u32593 \u26684 
    z_values = np.linspace(0, total_length, res_vertical)
    theta_values = np.linspace(0, 2 * np.pi, res_angular)
    
    # \uc0\u21019 \u24314 \u32593 \u26684 \u22352 \u26631 \u30697 \u38453 
    Z, THETA = np.meshgrid(z_values, theta_values)
    
    # \uc0\u21021 \u22987 \u21270 \u21322 \u24452 \u30697 \u38453 
    R = np.zeros_like(Z)
    
    # --- \uc0\u20960 \u20309 \u24418 \u29366 \u36923 \u36753  ---
    
    for i in range(res_angular):
        for j in range(res_vertical):
            z = Z[i, j]
            angle = THETA[i, j]
            
            current_radius = body_radius
            
            # 1. \uc0\u24213 \u37096  Apex (\u23376 \u24377 \u22836 \u24418 \u29366 )
            if z < apex_length:
                # \uc0\u20351 \u29992 \u26925 \u22278 \u26041 \u31243 \u27169 \u25311 \u23376 \u24377 \u22836 \u22278 \u24359 
                # (z/apex_length)^2 + (r/body_radius)^2 = 1
                # r = body_radius * sqrt(1 - ((apex_length - z)/apex_length)^2)
                # \uc0\u36825 \u37324 \u31245 \u24494 \u35843 \u25972 \u26354 \u32447 \u20351 \u20854 \u26356 \u20687 \u22270 \u29255 \u20013 \u30340 \u38045 \u22278 
                ratio = z / apex_length
                scale_factor = np.sqrt(1 - (1 - ratio)**2)
                current_radius = body_radius * scale_factor
                
            # 2. \uc0\u39030 \u37096  Collar (\u20809 \u28369 \u39048 \u22280 )
            elif z > (total_length - collar_height):
                # \uc0\u32447 \u24615 \u25554 \u20540 \u25110 \u36731 \u24494 \u24359 \u24230 \u25554 \u20540 \u65292 \u20174  body_radius \u36807 \u28193 \u21040  collar_top_radius
                h_in_collar = z - (total_length - collar_height)
                progress = h_in_collar / collar_height
                # \uc0\u22270 \u29255 \u20013 \u39048 \u22280 \u26159 \u25193 \u21475 \u30340 
                current_radius = body_radius + (collar_top_radius - body_radius) * progress
            
            # 3. \uc0\u20013 \u38388 \u20027 \u20307  (\u34746 \u32441 \u21306 \u22495 )
            else:
                current_radius = body_radius
            
            # --- \uc0\u28155 \u21152 \u34746 \u26059 \u34746 \u32441  ---
            # \uc0\u20165 \u22312 \u20027 \u20307 \u21306 \u22495 \u28155 \u21152 \u34746 \u32441 \u65292 \u19988 \u22312 \u20004 \u31471 \u28129 \u20837 \u28129 \u20986 
            if thread_start_z < z < thread_end_z:
                # \uc0\u34746 \u26059 \u30456 \u20301 : z/pitch - angle/2pi
                phase = (z / thread_pitch) - (angle / (2 * np.pi))
                
                # \uc0\u34746 \u32441 \u27874 \u24418  (\u31867 \u20284 \u20110 \u26799 \u24418 \u27874 \u25110 \u24179 \u28369 \u19977 \u35282 \u27874 )
                # \uc0\u20351 \u29992 \u27491 \u24358 \u27874 \u30340 \u21464 \u20307 \u26469 \u27169 \u25311 \u38160 \u21033 \u30340 \u34746 \u32441 
                wave = np.sin(2 * np.pi * phase)
                
                # \uc0\u35753 \u34746 \u32441 \u26356 \u26377 \u26865 \u35282  (\u27169 \u25311 \u20999 \u21066 )
                if wave > 0:
                    thread_mod = wave ** 0.8  # \uc0\u39030 \u37096 \u31245 \u22278 
                else:
                    thread_mod = -((-wave) ** 2.5) # \uc0\u35895 \u24213 \u36739 \u23485 \u19988 \u24179 \u28369 
                
                # \uc0\u35745 \u31639 \u28129 \u20837 \u28129 \u20986 \u22240 \u23376  (Fade factor)
                # \uc0\u36825 \u26679 \u34746 \u32441 \u22312 \u24320 \u22987 \u21644 \u32467 \u26463 \u26102 \u19981 \u20250 \u31361 \u28982 \u26029 \u35010 
                fade_dist = 1.0 # 1mm \uc0\u30340 \u36807 \u28193 \u21306 
                fade = 1.0
                
                if z < thread_start_z + fade_dist:
                    fade = (z - thread_start_z) / fade_dist
                elif z > thread_end_z - fade_dist:
                    fade = (thread_end_z - z) / fade_dist
                
                fade = np.clip(fade, 0, 1)
                
                # \uc0\u24212 \u29992 \u34746 \u32441 \u28145 \u24230 
                current_radius += thread_depth * thread_mod * fade

            R[i, j] = current_radius

    # --- \uc0\u22352 \u26631 \u36716 \u25442  (\u26609 \u22352 \u26631  -> \u31515 \u21345 \u23572 \u22352 \u26631 ) ---
    X = R * np.cos(THETA)
    Y = R * np.sin(THETA)
    
    # --- \uc0\u26500 \u24314 \u19977 \u35282 \u38754 \u29255  ---
    # \uc0\u25105 \u20204 \u38656 \u35201 \u23558 \u32593 \u26684 \u28857 \u36830 \u25509 \u25104 \u19977 \u35282 \u24418 
    # \uc0\u36825 \u26159 \u19968 \u20010 \u35268 \u21017 \u32593 \u26684 \u65292 \u27599 \u20010 \u30697 \u24418 \u21333 \u20803 \u30001 \u20004 \u20010 \u19977 \u35282 \u24418 \u32452 \u25104 
    
    faces = []
    
    for i in range(res_angular):
        for j in range(res_vertical - 1):
            # \uc0\u32034 \u24341 \u35745 \u31639  (\u27880 \u24847 \u22788 \u29702 \u22278 \u21608 \u38381 \u21512 )
            i_next = (i + 1) % res_angular
            
            # \uc0\u24403 \u21069 \u23618 \u30340 \u20004 \u20010 \u28857 \u21644 \u19979 \u19968 \u23618 \u30340 \u20004 \u20010 \u28857 
            # P1 -- P2
            # |  /  |
            # P3 -- P4
            
            # \uc0\u39030 \u28857 \u22352 \u26631 
            v1 = [X[i, j], Y[i, j], Z[i, j]]
            v2 = [X[i_next, j], Y[i_next, j], Z[i_next, j]]
            v3 = [X[i, j+1], Y[i, j+1], Z[i, j+1]]
            v4 = [X[i_next, j+1], Y[i_next, j+1], Z[i_next, j+1]]
            
            # \uc0\u28155 \u21152 \u20004 \u20010 \u19977 \u35282 \u24418 
            faces.append([v1, v2, v3]) # \uc0\u19979 \u19977 \u35282 
            faces.append([v2, v4, v3]) # \uc0\u19978 \u19977 \u35282 

    # --- \uc0\u23553 \u39030  (\u39030 \u37096 \u21644 \u24213 \u37096 ) ---
    # \uc0\u24213 \u37096 \u23454 \u38469 \u19978 \u26159 \u19968 \u20010 \u28857 (\u22240 \u20026 \u21322 \u24452 \u36235 \u36817 \u20110 0)\u65292 \u20294 \u20026 \u20102 \u40065 \u26834 \u24615 \u25105 \u20204 \u21487 \u20197 \u28155 \u21152 \u19968 \u20010 \u25159 \u24418 
    # \uc0\u39030 \u37096 \u26159 \u19968 \u20010 \u22278 \u30424 
    
    # \uc0\u39030 \u37096 \u23553 \u30422 
    top_z_idx = res_vertical - 1
    center_top = [0, 0, total_length]
    for i in range(res_angular):
        i_next = (i + 1) % res_angular
        v1 = [X[i, top_z_idx], Y[i, top_z_idx], Z[i, top_z_idx]]
        v2 = [X[i_next, top_z_idx], Y[i_next, top_z_idx], Z[i_next, top_z_idx]]
        # \uc0\u27880 \u24847 \u27861 \u32447 \u26041 \u21521 \u21521 \u19978 
        faces.append([center_top, v2, v1])

    # \uc0\u24213 \u37096 \u23553 \u30422  (\u34429 \u28982 \u21322 \u24452 \u25509 \u36817 0\u65292 \u20294 \u20026 \u20102 \u38381 \u21512 \u27969 \u24418 \u65292 \u21152 \u19978 \u23553 \u30422 )
    bottom_z_idx = 0
    center_bottom = [0, 0, 0]
    for i in range(res_angular):
        i_next = (i + 1) % res_angular
        v1 = [X[i, bottom_z_idx], Y[i, bottom_z_idx], Z[i, bottom_z_idx]]
        v2 = [X[i_next, bottom_z_idx], Y[i_next, bottom_z_idx], Z[i_next, bottom_z_idx]]
        # \uc0\u27880 \u24847 \u27861 \u32447 \u26041 \u21521 \u21521 \u19979 
        faces.append([center_bottom, v1, v2])

    # --- \uc0\u36716 \u25442 \u20026  numpy-stl \u26684 \u24335  ---
    faces_np = np.array(faces)
    data = np.zeros(len(faces), dtype=mesh.Mesh.dtype)
    data['vectors'] = faces_np
    
    implant_mesh = mesh.Mesh(data)
    
    # \uc0\u20445 \u23384 \u25991 \u20214 
    implant_mesh.save(filename)
    print(f"Generated STL file: {filename}")
    print(f"Dimensions: diameter ~{body_radius*2}mm, length {total_length}mm")

if __name__ == "__main__":
    generate_implant_stl("dental_implant.stl")
