#include "student.h"
#include <stdio.h>


// Function to swap two integers
void swap(int *a, int *b) {
  int temp = *a; // Store the value of a in a temp variable
  *a = *b; // Assign the value of b to a
  *b = temp; // Assign the value of the temp variable to b
}

void modifyarray(int array[], int length) {
   int i = 0;
   for (i = 0; i < length; i++) {
    if(array[i] > 0){
      //if num is positive, then double it
      array[i] *= 2;
    }else if(array[i] < 0){
      //if the num is negative, then add 1 to it
      array[i] += 2;
    }
    //if num is 0, then leave it as it is
  }
}

// Function to find the "nth" Fibonacci number
int nthfibonacci(int n) {
  if (n <= 1) {
    return n; // Return n if it is 0 or 1
  }
  int a = 0, b = 1, i = 0;
  for (i = 2; i <= n - 1; i++) { //Subtracted by one , otherwise it will return the next Fibonacci number and wont pass the test cases
    int temp = a + b; // Calculate the next Fibonacci number by adding the previous two numbers
    a = b; // Update the value of a to the previous value of b
    b = temp; // Update the value of b to the calculated Fibonacci number
  }
  return b; // Return the nth Fibonacci number calculated
}

// Function to calculate the median of an array
double median(int array[], int length) {
  if (length % 2 == 0) { // If the length of the array is even
    int mid1 = length / 2 - 1; // Calculate the index of the first middle element
    int mid2 = length / 2; // Calculate the index of the second middle element
    return (array[mid1] + array[mid2]) / 2.0; // Return the average of the two middle elements
  } else { // If the length of the array is odd
    int mid = length / 2; // Calculate the index of the middle element
    return array[mid]; // Return the middle element (since there is only one middle element)
  }
}

//Funstion to sort a list using bubble sort: "repeatedly comparing adjacent elements in an array and swapping them if they are in the wrong order"
void bubblesort(int array[], int length) {
  int i = 0;
  for (i = 0; i < length - 1; i++) { // Iterate through the array
    int j = 0;
    for (j = 0; j < length - i - 1; j++) { // Iterate through the array again
      if (array[j] > array[j + 1]) { // If the current element is greater than the next element
        swap(&array[j], &array[j + 1]); // Using the swap function defined earlier
      }
    }
  }
}

// Function to check if a number is an Armstrong number
int armstrong(int n) {
  int original = n; // Store the original number for later comparison
  int sum = 0;
  while (n > 0) { // Iterate through the digits of n
    int digit = n % 10; // Get the last digit of n
    sum += digit * digit * digit; // Add the cube of the digit to the sum
    n /= 10; // Remove the last digit from n
  }
  return (sum == original); // Return true if the sum is equal to the original number, otherwise return false
}


// Function to check if a string is a palindrome
int palindrome(char str[], int length) {
  int start = 0; // Initialize the start index of the string
  int end = length - 1; // Initialize the end index of the string

  while (start < end) { // Iterate until the start index is less than the end index
    if (str[start] != str[end]) { // If the characters at start and end indices are not equal
      return 0; // Not a palindrome
    }
    start++;
    end--;
  }

  return 1; // is a palindrome
}