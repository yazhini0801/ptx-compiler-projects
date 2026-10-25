kernel choose(float* out, float x, float y, int flag) {
    if (flag == 0) {
        out[0] = x;
    } else {
        out[0] = y;
    }
}
