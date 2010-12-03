int main() {
  int a, b, c, *p;
  scanf("%d", &a);  // force a to be stored in memory not a register
  b = a * 2;        // useless
  c = a + 4;
  printf("c: %d\n", c);
}
