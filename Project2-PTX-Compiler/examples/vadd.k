// Vector addition: out[i] = a[i] + b[i].
kernel vadd(float* a, float* b, float* out, int n) {
    int i = gid();
    if (i < n) {
        out[i] = a[i] + b[i];
    }
}
