// Single-precision SAXPY: y[i] = alpha * x[i] + y[i].
kernel saxpy(float* x, float* y, float alpha, int n) {
    int i = gid();
    if (i < n) {
        y[i] = alpha * x[i] + y[i];
    }
}
