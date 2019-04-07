struct val {
  int x;
};
struct node {
  int val1;
  struct val *vv;
  struct val v;
  int nodeArr[13];
};

int main() {
  // struct node *arr[10];
  // arr[1]->val1 = 889;
  // arr[2]->nodeArr[5] = 33;

  int arr[10][10];
   arr[4][7] = 17;
  // ---------
  // struct node n;
  // struct val v;
  // n.val1 = 40;
  // n.v.x = 10;
  // n.vv = &v;
  // n.vv->x = 30;


// ----------------
//  int x = 10;
//  int arr[2][x];
}
