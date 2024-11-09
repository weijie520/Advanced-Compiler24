int main() {
  int i;
  int A[20], B[20], C[20];
  for (i = 10; i < 21; i++) {
    A[2 * i - 1] = C[i];
    B[i] = A[4 * i - 7];
  }
  return 0;
}