// Small representative kernels compiled with NVIDIA NVCC 12.9.1 on Compiler Explorer.
extern "C" __global__ void vector_add(const float* a, const float* b, float* c, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) c[i] = a[i] + b[i];
}

extern "C" __global__ void clamp_scale(float* out, const float* in, int n, float scale,
                                        float lower, float upper) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    float value = in[i] * scale;
    value = value < lower ? lower : value;
    value = value > upper ? upper : value;
    out[i] = value;
}

extern "C" __global__ void triangular_sum(int* out, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    int sum = 0;
#pragma unroll 1
    for (int j = 0; j <= i; ++j) sum += j;
    out[i] = sum;
}

extern "C" __global__ void nested_sum(int* out, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    int sum = 0;
#pragma unroll 1
    for (int j = 0; j < 3; ++j) {
#pragma unroll 1
        for (int k = 0; k < 2; ++k) sum += i + j + k;
    }
    out[i] = sum;
}
