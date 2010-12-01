int main() {
  int x, y, z;

  x = 12;
  y = x + 22;  /* load value of x that was just stored */
  z = y + 33;  /* load value of y that was just stored */
  return z;
}
