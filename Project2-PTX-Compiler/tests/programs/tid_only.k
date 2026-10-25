kernel tid_only(int* out) {
    int local_id = tid();
    out[local_id] = local_id;
}
