import ctypes
import os
import math
import numpy as np
import pytest
from typing import List, Tuple

class Vector3D(ctypes.Structure):
    """
    Memory-aligned Cartesian coordinate structure for Foreign Function Interface operations.
    
    Attributes
    ----------
    x : float
        X-axis spatial coordinate.
    y : float
        Y-axis spatial coordinate.
    z : float
        Z-axis spatial coordinate.
    w : float
        Homogeneous coordinate scalar.
    """
    _fields_ = [
        ("x", ctypes.c_float),
        ("y", ctypes.c_float),
        ("z", ctypes.c_float),
        ("w", ctypes.c_float),
    ]

class MathematicalEngineValidator:
    """
    Validates the custom C implementation of the 3D telemetry graphics engine 
    against NumPy's BLAS/LAPACK optimized backend to ensure floating-point stability.
    """
    def __init__(self, lib_path: str):
        self.abs_path = os.path.abspath(lib_path)
        if not os.path.exists(self.abs_path):
            raise FileNotFoundError(f"Shared object not found at {self.abs_path}")
        
        self.lib = ctypes.CDLL(self.abs_path)
        
        self.lib.Graphics3D_ComputeRotationMatrix.argtypes = [
            ctypes.POINTER(ctypes.c_float),
            ctypes.c_float,
            ctypes.c_float,
            ctypes.c_float
        ]
        self.lib.Graphics3D_ComputeRotationMatrix.restype = None

        self.lib.Graphics3D_RotateVertices.argtypes = [
            ctypes.POINTER(Vector3D),
            ctypes.POINTER(Vector3D),
            ctypes.POINTER(ctypes.c_float),
            ctypes.c_size_t
        ]
        self.lib.Graphics3D_RotateVertices.restype = None

@pytest.fixture(scope="module")
def math_validator() -> MathematicalEngineValidator:
    possible_paths = [
        "libengine_mock_shared.so",
        "../build/libengine_mock_shared.so",
        "./build/libengine_mock_shared.so"
    ]
    for path in possible_paths:
        if os.path.exists(path):
            return MathematicalEngineValidator(path)
    pytest.fail("SIL Testing Library not found. Execute CMake build first.")

def compute_numpy_rotation_matrix(rx: float, ry: float, rz: float) -> np.ndarray:
    """
    Generates a mathematically rigorous 3x3 Euler rotation matrix using NumPy.
    Order of extrinsic rotation: Z * Y * X
    """
    cx, sx = np.cos(rx), np.sin(rx)
    cy, sy = np.cos(ry), np.sin(ry)
    cz, sz = np.cos(rz), np.sin(rz)

    rot_x = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    rot_y = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    rot_z = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])

    return rot_z @ rot_y @ rot_x

def test_rotation_matrix_precision(math_validator: MathematicalEngineValidator):
    """
    Validates the C implementation of Euler angle rotation matrix generation.
    Checks deterministic output within a 1e-5 floating point epsilon.
    """
    angles = [(0.0, 0.0, 0.0), (math.pi/4, math.pi/6, math.pi/3), (-1.5, 2.7, 0.8)]
    
    for rx, ry, rz in angles:
        c_matrix = (ctypes.c_float * 9)()
        math_validator.lib.Graphics3D_ComputeRotationMatrix(c_matrix, rx, ry, rz)
        c_result = np.array(c_matrix).reshape(3, 3)

        py_result = compute_numpy_rotation_matrix(rx, ry, rz)

        np.testing.assert_allclose(
            c_result, py_result, 
            atol=1e-5, rtol=1e-5, 
            err_msg=f"Matrix misalignment detected at angles ({rx}, {ry}, {rz})"
        )

def test_vertex_transformation_stability(math_validator: MathematicalEngineValidator):
    """
    Validates the matrix-vector multiplication stability, ensuring that
    memory alignment restrictions (specifically for ARM NEON SIMD) do not corrupt data.
    """
    rx, ry, rz = 0.5, -0.3, 1.2
    c_matrix = (ctypes.c_float * 9)()
    math_validator.lib.Graphics3D_ComputeRotationMatrix(c_matrix, rx, ry, rz)

    test_points = [
        (-1.0, -1.0, -1.0), (1.0, -1.0, -1.0), 
        (1.0, 1.0, -1.0), (-1.0, 1.0, -1.0)
    ]
    count = len(test_points)
    
    input_array = (Vector3D * count)()
    output_array = (Vector3D * count)()
    
    for i, (px, py, pz) in enumerate(test_points):
        input_array[i] = Vector3D(x=px, y=py, z=pz, w=1.0)

    math_validator.lib.Graphics3D_RotateVertices(input_array, output_array, c_matrix, count)

    py_matrix = compute_numpy_rotation_matrix(rx, ry, rz)
    
    for i, (px, py, pz) in enumerate(test_points):
        vec = np.array([px, py, pz])
        py_rotated = py_matrix @ vec
        
        c_rotated = np.array([output_array[i].x, output_array[i].y, output_array[i].z])
        
        np.testing.assert_allclose(
            c_rotated, py_rotated, 
            atol=1e-5, rtol=1e-5,
            err_msg=f"Vertex transformation failed at vertex {i}"
        )
