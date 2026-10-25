// Each thread computes the sum 0 + ... + (n - 1), then stores it.
kernel sum_to(int* out, int n) {
    int i = gid();
    if (i < n) {
        int total = 0;
        int k = 0;
        while (k < n) {
            total = total + k;
            k = k + 1;
        }
        out[i] = total;
    }
}
