int main() {
  int x = 10;
  switch(x) {
    {
      case 10:
        return 111;
        {
      case 20:
        return 222;
        }
    }

    {
      default:
        return 333;
    }
  }

  switch(x)
  default:
    return 444;

  switch(x) {
    case 1:
      {
    case 2:
    case 3:
    case 4:
      return 445;
      }
  }

  return 0;
}
