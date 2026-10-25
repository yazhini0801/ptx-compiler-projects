kernel for_logical(int* out, int n) {
    int sum = 0;
    for (int i = 0; i < n; i = i + 1) {
        if ((i < n && i >= 0) || n == 0) {
            sum = sum + i;
        }
    }
    out[0] = sum;
}
