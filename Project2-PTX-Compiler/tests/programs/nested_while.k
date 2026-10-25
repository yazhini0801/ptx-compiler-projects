kernel nested_while(int* out, int n) {
    int i = 0;
    int total = 0;
    while (i < n) {
        int j = 0;
        while (j < 2) {
            total = total + i;
            j = j + 1;
        }
        i = i + 1;
    }
    out[tid()] = total;
}
