#
# Copyright (C) 2023, Inria
# GRAPHDECO research group, https://team.inria.fr/graphdeco
# All rights reserved.
#
# This software is free for non-commercial, research and evaluation use 
# under the terms of the LICENSE.md file.
#
# For inquiries contact  george.drettakis@inria.fr
#

import os
import numpy as np
from argparse import ArgumentParser
import json

from pose_utils import generate_ellipse_path, getWorld2View2
from pose_utils import View

def load_camera_poses(camera_file):
    with open(camera_file, 'r') as f:
        camera_data = json.load(f)
    return camera_data

def generate_ellipse_views(args):
    camera_data = load_camera_poses(args.camera)
    fx, fy = camera_data[0]['fx'], camera_data[0]['fy']
    W, H = camera_data[0]['width'], camera_data[0]['height']
    
    views = []
    for pose in camera_data:
        R = np.array(pose['rotation']).T
        T = np.array(pose['position'])
        view = View(R, T)
        views.append(view)
    pose_ellipse = generate_ellipse_path(views, n_frames=100)
    
    camera_infos = []
    for idx, pose in enumerate(pose_ellipse):
        camera_info = {
            'id': idx,
            'img_name': f"ellipse_{idx:03d}.png",
            'width': W,
            'height': H,
            'rotation': pose[:3, :3].tolist(),
            'position': pose[:3, 3].tolist(),
            'fy': fy,
            'fx': fx
        }
        camera_infos.append(camera_info)
    
    with open(args.output, 'w') as f:
        json.dump(camera_infos, f, indent=4)
        
    return camera_infos

def generate_circular_views(args, radius=0.5, n_frames=240):
    camera_data = load_camera_poses(args.camera)
    fx, fy = camera_data[0]['fx'], camera_data[0]['fy']
    W, H = camera_data[0]['width'], camera_data[0]['height']
    
    camera_infos = []
    for idx, pose in enumerate(camera_data):
        R = np.array(pose['rotation']).T
        T = np.array(pose['position'])
        
        angle = 2 * np.pi * idx / n_frames
        translate_x = radius * np.cos(angle)
        translate_y = radius * np.sin(angle)
        translate_z = 0
        translate = np.array([translate_x, translate_y, translate_z])
        
        W2C = getWorld2View2(R, T, translate)
        camera_info = {
            'id': idx,
            'img_name': f"circular_{idx:03d}.png",
            'width': W,
            'height': H,
            'rotation': W2C[:3, :3].tolist(),
            'position': W2C[:3, 3].tolist(),
            'fy': fy,
            'fx': fx
        }
        camera_infos.append(camera_info)
    
    with open(args.output, 'w') as f:
        json.dump(camera_infos, f, indent=4)
        
    return camera_infos

if __name__ == "__main__":
    # Set up command line argument parser
    parser = ArgumentParser(description="Testing script parameters")
    parser.add_argument("--camera", default="cameras.json", type=str)
    parser.add_argument("--output", default="out.json", type=str)
    args = parser.parse_args()
    print("Read Camera " + args.camera)
    
    # generate_ellipse_views(args)
    generate_circular_views(args)
