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

from pose_utils import generate_ellipse_path, generate_spherical_sample_path, generate_spiral_path, generate_spherify_path,  gaussian_poses, circular_poses
from pose_utils import View

def load_camera_poses(camera_file):
    with open(camera_file, 'r') as f:
        camera_data = json.load(f)
    return camera_data

def generate_views(args):
    camera_data = load_camera_poses(args.camera)
    fx, fy = camera_data[0]['fx'], camera_data[0]['fy']
    
    views = []
    for pose in camera_data:
        W2C = np.eye(4)
        W2C[:3, :3] = np.array(pose['rotation'])
        W2C[:3, 3] = np.array(pose['position'])
        C2W = np.linalg.inv(W2C)
        R = C2W[:3, :3].T
        T = C2W[:3, 3]
        
        view = View(R, T)
        views.append(view)
    pose_ellipse = generate_ellipse_path(views, n_frames=100)
    
    camera_infos = []
    for idx, pose in enumerate(pose_ellipse):
        R = pose[:3, :3]
        T = pose[:3, 3] 
        camera_info = {
            'id': idx,
            'img_name': f"ellipse_{idx:03d}.png",
            'width': 720,
            'height': 1080,
            'rotation': pose[:3, :3].tolist(),
            'position': pose[:3, 3].tolist(),
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
    
    generate_views(args)
