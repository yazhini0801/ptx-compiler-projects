#!/usr/bin/env python3
"""Run the generated vadd PTX on a Colab NVIDIA GPU and check its output."""
import pathlib

import numpy as np
from cuda.bindings import driver


def checked(result):
    status = result[0]
    if status != driver.CUresult.CUDA_SUCCESS:
        error = driver.cuGetErrorName(status)
        name = error[1] if error[0] == driver.CUresult.CUDA_SUCCESS else str(status)
        raise RuntimeError(f"CUDA Driver API call failed: {name}")
    if len(result) == 1:
        return None
    return result[1] if len(result) == 2 else result[1:]


def main():
    root = pathlib.Path(__file__).resolve().parents[1]
    ptx_path = root / "examples" / "vadd.ptx"
    if not ptx_path.is_file():
        raise SystemExit(f"PTX file not found: {ptx_path}")

    count = 1024
    block_size = 128
    a = np.arange(count, dtype=np.float32)
    b = (np.arange(count, dtype=np.float32) * 2.0) + 1.0
    output = np.zeros(count, dtype=np.float32)
    expected = a + b
    size = a.nbytes

    checked(driver.cuInit(0))
    device = checked(driver.cuDeviceGet(0))
    # Recent cuda-python bindings expose the full Driver API signature,
    # including the optional context-creation parameter structure.
    context = checked(driver.cuCtxCreate(None, 0, device))
    module = None
    device_a = device_b = device_out = None
    try:
        name = checked(driver.cuDeviceGetName(128, device))
        print(f"Using GPU: {name.decode(errors='replace') if isinstance(name, bytes) else name}")
        # cuModuleLoadData accepts the PTX image itself (NUL-terminated),
        # rather than a host pointer to a NumPy character array.
        ptx = ptx_path.read_bytes() + b"\0"
        module = checked(driver.cuModuleLoadData(ptx))
        kernel = checked(driver.cuModuleGetFunction(module, b"vadd"))

        device_a = checked(driver.cuMemAlloc(size))
        device_b = checked(driver.cuMemAlloc(size))
        device_out = checked(driver.cuMemAlloc(size))
        checked(driver.cuMemcpyHtoD(device_a, a.ctypes.data, size))
        checked(driver.cuMemcpyHtoD(device_b, b.ctypes.data, size))

        arg_a = np.array([int(device_a)], dtype=np.uint64)
        arg_b = np.array([int(device_b)], dtype=np.uint64)
        arg_out = np.array([int(device_out)], dtype=np.uint64)
        arg_count = np.array([count], dtype=np.uint32)
        kernel_args = np.array(
            [arg.ctypes.data for arg in (arg_a, arg_b, arg_out, arg_count)],
            dtype=np.uint64,
        )
        grid_size = (count + block_size - 1) // block_size
        checked(
            driver.cuLaunchKernel(
                kernel,
                grid_size, 1, 1,
                block_size, 1, 1,
                0, 0,
                kernel_args.ctypes.data,
                0,
            )
        )
        checked(driver.cuCtxSynchronize())
        checked(driver.cuMemcpyDtoH(output.ctypes.data, device_out, size))

        if not np.array_equal(output, expected):
            mismatch = int(np.flatnonzero(output != expected)[0])
            raise AssertionError(
                f"vadd mismatch at index {mismatch}: expected {expected[mismatch]}, got {output[mismatch]}"
            )
        print(f"PASS: vadd produced the expected sums for all {count} elements.")
    finally:
        if device_a is not None:
            checked(driver.cuMemFree(device_a))
        if device_b is not None:
            checked(driver.cuMemFree(device_b))
        if device_out is not None:
            checked(driver.cuMemFree(device_out))
        if module is not None:
            checked(driver.cuModuleUnload(module))
        checked(driver.cuCtxDestroy(context))


if __name__ == "__main__":
    main()
