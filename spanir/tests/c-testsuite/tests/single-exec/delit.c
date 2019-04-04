struct val {
  int z;
};
struct node {
  int x;
  struct val v;
};
int sum(int a, int b) {
  return a + b;
}

int main() {
  int x, y;
  x = 10;

  y = 1 ,2, 3, ++x, 5;

  return x + y;

//  sum(10, 20);
//  ---------
//  int arr[2][3];
//  arr[0][1] = 2;
//  int *x;
//  x = &(arr+1)[0][1];
//  int y = x[2];
//  ---------
//  struct node n;
//  struct node *n1;
//  n1 = &n;
//  n.v.z = 20;
//  (n1 +1 -1)->v.z = 30;
// -----------  
//  int x, *y;
//  float f;
//  y = &x;
//  x = -(x + 10);
//  x = 10;
//  x = x++ + --x + x;
//  f = (float)x;
// --------------
//  int x = 10;
//  switch(x) { {
//    if (x) {
//    case 10: {
//      x = 2;
//      break;
//             }
//    case 30:
//             x = 3;
//             break;
//    default:
//      x = 20;
//      break;
//    }
//  }
//  }
//-------------------
//   int y = 10;
//   int x = y && y || y;
//   int z = y ? x : y;
// hello:
//   for (;;) {
//     break;
//   }
//   do {
//     x = x - 1;
//     continue;
//     x = x + 1;
//   } while(x);
//   return 0;
//   z = y;
}
