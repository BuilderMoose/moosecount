#pragma once

// The counting categories, as specified in design/0.3.0-requirements.md.
// Every line is blank, or some combination of code, comment and format.
struct CountTotals
{
  long totalLines = 0;
  long blankLines = 0;
  long codeLines = 0;
  long formatLines = 0;
  long commentLines = 0;
  long fileCount = 0;
};
