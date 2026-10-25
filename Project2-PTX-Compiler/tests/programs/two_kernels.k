kernel first(int* out) {
    out[gid()] = 1;
}

kernel second(float* out) {
    out[tid()] = 2.5;
}
