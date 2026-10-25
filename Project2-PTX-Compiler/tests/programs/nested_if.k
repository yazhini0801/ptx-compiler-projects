kernel nested_if(int* out, int a, int b) {
    int result = 0;
    if (a > 0) {
        if (b >= a) {
            result = b;
        } else {
            result = a;
        }
    }
    out[gid()] = result;
}
