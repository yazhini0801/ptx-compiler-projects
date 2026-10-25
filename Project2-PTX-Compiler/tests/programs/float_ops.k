kernel float_ops(float* out) {
    float value = -2.5 * 2.0 + 1.25 / 5.0;
    out[gid()] = value;
}
