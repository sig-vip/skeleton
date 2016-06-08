/*  This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

// Copyright (C) 2015 Ivan Shaydurov

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <windows.h>
#include <string>
#include <ddmio.hpp>
#include <arageli/arageli.hpp>

using namespace Arageli;
using std::cin;
using std::wcout;
using std::wcerr;
using std::endl;

#define SKELETONPATH TEXT("\"skeleton32.exe\" ")
#define DEFAULTOUTPUTFILENAME TEXT("LinearDiscriminantFunctions")
#define DEFAULTOUTPUTFILENAMEEXT TEXT(".ine")
#define DEFAULTSKELETONOUTPUTFILENAMEEXT TEXT(".out")
#define DEFAULTFUNCTIONSOUTPUTFILENAMEEXT TEXT(".func")
#define DEFINE_OUTPUTFILENAME						\
	t_string outputfilename;					\
												\
	if (!inputfilename.size())					\
	{											\
		outputfilename = DEFAULTOUTPUTFILENAME;	\
	}											\
	else										\
	{											\
		outputfilename = inputfilename;			\
	}

#ifdef _UNICODE
#define to_basic_string std::to_wstring
#else
#define to_basic_string std::to_string
#endif

typedef std::basic_string<TCHAR> t_string;

void print_help()
{
	wcout << "Linear Discriminant Functions\n"
		  << "USAGE: LDF [inputfilename] [options]\n"
		  << "OPTIONS:\n"
		  << "Mode: {--importimages} / --importfunctions\n"
		  << "Arithmetic: {--bigint} / --int / --float / --rational\n"
		  << "Inp.option: --inputfromstdin / {--noinputfromstdin}  " << endl;
}

template<class T>
void construct_functions(matrix<T> &functions, const matrix<T> &func, size_t ii)
{
	typedef typename matrix<T>::difference_type index;

	index p = func.nrows();
	index q = func.ncols();

	for (index i = 0; i < p; ++i)
	{
		for (index j = 0; j < q; ++j)
		{
			functions(ii, j) += func(i, j);
		}
	}
}

template <class T>
void read_and_prepare_matrix_and_call_skeleton(matrix<T> &img, matrix<T> &functions, t_string arith,
	int inputfromstdin, t_string inputfilename, std::istream &inputfile, int &errorflag)
{
	try
	{
		if (inputfromstdin)
			matrix_input(cin, img);
		else
			matrix_input(inputfile, img);
	}
	catch (...)
	{
		wcerr << "Input error. Program terminated." << endl;
		errorflag = 1;
		return;
	}

	std::fstream outputfile;

	typedef typename matrix<T>::difference_type index;

	index m = img.nrows();
	index n = img.ncols();

	// Matrix ine is used to construct the system of inequalities for the linear discriminant function
	matrix<T> ine;

	ine.resize(m, n + 1);
	functions.resize(m, n + 1);

	// ii is the index of considered function
	for (size_t ii = 0; ii < m; ++ii)
	{
		for (index i = 0; i < m; ++i)
		{
			for (index j = 0; j < n; ++j)
			{
				if (i == ii)
				{
					ine(i, j) = img(i, j);
				}
				else
				{
					ine(i, j) = -1 * img(i, j);
				}
			}

			if (i == ii)
			{
				ine(i, n) = 1;
			}
			else
			{
				ine(i, n) = -1;
			}
		}

		DEFINE_OUTPUTFILENAME

		outputfilename += to_basic_string(ii) + DEFAULTOUTPUTFILENAMEEXT;

		outputfile.open(outputfilename, std::ios::out);

		if (outputfile.bad() || !outputfile.is_open())
		{
			wcerr << "LinearDiscriminantFunctions Error: can not open file " << outputfilename
				  << " for output. Program terminated." << endl;
			errorflag = 1;
			return;
		}

		matrix_output(outputfile, ine, COLUMNSEQUALWIDTH);
		outputfile.close();

		// Solving the system of inequalities by using skeleton program
		STARTUPINFO si;
		PROCESS_INFORMATION pi;

		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		LPTSTR szCmdLine = _tcsdup((t_string(SKELETONPATH) + outputfilename
						 + TEXT(" --nosummary --nolog ") + arith).c_str());
		
		// Start the child process. 
		if (!CreateProcess(NULL,   // No module name (use command line)
			szCmdLine,      // Command line
			NULL,           // Process handle not inheritable
			NULL,           // Thread handle not inheritable
			FALSE,          // Set handle inheritance to FALSE
			0,              // No creation flags
			NULL,           // Use parent's environment block
			NULL,           // Use parent's starting directory 
			&si,            // Pointer to STARTUPINFO structure
			&pi)            // Pointer to PROCESS_INFORMATION structure
			)
		{
			wprintf(L"CreateProcess failed (%d).\nProgram terminated.\n", GetLastError());
			errorflag = 1;
			return;
		}

		// Wait until child process exits.
		WaitForSingleObject(pi.hProcess, INFINITE);

		// Close process and thread handles. 
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		free(szCmdLine);

		// Parsing skeleton output file and constructing linear discriminant functions
		outputfilename += DEFAULTSKELETONOUTPUTFILENAMEEXT;

		outputfile.open(outputfilename, std::ios::in);

		if (outputfile.bad() || !outputfile.is_open())
		{
			wcerr << "LinearDiscriminantFunctions Error: can not open file " << outputfilename
				  << " for input. Program terminated." << endl;
			errorflag = 1;
			return;
		}

		try
		{
			std::stringstream StreamBas, StreamExt;
			std::string str, buff;
			const std::string bas("* Basis:\n"), ext("* Extreme rays:\n");

			while (std::getline(outputfile, buff))
			{
				str += buff + "\n";
			}

			str.erase(str.find(bas), bas.size());
			size_t found = str.find(ext);
			buff.assign(str, 0, found);
			StreamBas << buff;

			matrix<T> func;

			matrix_input(StreamBas, func);
			construct_functions(functions, func, ii);
			
			str.erase(0, found + ext.size());
			StreamExt << str;

			matrix_input(StreamExt, func);
			construct_functions(functions, func, ii);
		}
		catch (...)
		{
			wcerr << "Input error. Image discrimination is inaccessible." << endl;
			errorflag = 1;
		}

		outputfile.close();
	}

	DEFINE_OUTPUTFILENAME

	outputfilename += DEFAULTFUNCTIONSOUTPUTFILENAMEEXT;

	outputfile.open(outputfilename, std::ios::out);

	if (outputfile.bad() || !outputfile.is_open())
	{
		wcerr << "LinearDiscriminantFunctions Error: can not open file " << outputfilename
			  << " for output. Program terminated." << endl;
		errorflag = 1;
		return;
	}

	matrix_output(outputfile, functions, COLUMNSEQUALWIDTH);
	outputfile.close();

	wcout << "Linear discriminant functions were created successfully." << endl;
}

template<class T>
void image_classification(matrix<T> &functions, int importimages, int inputfromstdin,
	t_string inputfilename, std::istream &inputfile, int &errorflag)
{
	if (!importimages)
	{
		try
		{
			if (inputfromstdin)
				matrix_input(cin, functions);
			else
				matrix_input(inputfile, functions);
		}
		catch (...)
		{
			wcerr << "Input error. Program terminated." << endl;
			errorflag = 1;
			return;
		}
	}

	std::string inputstring;

	wcout << "Would you like to test an image (y/n)? ";
	cin >> inputstring;

	if (inputstring == "y" || inputstring == "Y")
	{
		typedef typename matrix<T>::difference_type index;

		matrix<T> inputimg;
		index m = functions.nrows();
		index n = functions.ncols() - 1;

		while (inputstring == "y" || inputstring == "Y")
		{
			wcout << "Enter image coordinates in " << n << "-space: ";

			inputimg.resize(1, n);

			try
			{
				for (index j = 0; j < n; ++j)
				{
					cin >> inputimg(0, j);
				}
			}
			catch (...)
			{
				wcerr << "Input error. Program terminated." << endl;
				errorflag = 1;
				return;
			}

			// Computation of the sign of linear discriminant functions for the image under consideration
			matrix<T> sign;

			sign.resize(m, 1);

			for (size_t i = 0; i < m; ++i)
			{
				for (size_t j = 0; j < n; ++j)
				{
					sign(i, 0) += inputimg(0, j) * functions(i, j);
				}

				sign(i, 0) += functions(i, n);
			}

			// Finding the class that ownes the considered image
			bool found = false;

			for (size_t i = 0; i < m; ++i)
			{
				bool flag = true;

				if (sign(i, 0) > 0)
				{
					for (size_t ii = 0; ii < i; ++ii)
					{
						if (sign(ii, 0) >= 0)
						{
							flag = false;
						}
					}

					for (size_t ii = i + 1; ii < m; ++ii)
					{
						if (sign(ii, 0) >= 0)
						{
							flag = false;
						}
					}
				}
				else
				{
					flag = false;
				}

				if (flag)
				{
					wcout << "The image under consideration belongs to the class " << i << endl;
					found = true;
					break;
				}
			}

			if (!found)
			{
				wcout << "The image under consideration does not belong to any of the classes" << endl;
			}

			wcout << "Would you like to test an image (y/n)? ";
			cin >> inputstring;
		}
	}
}

template<class T>
void handling_matrices(matrix<T> &img, matrix<T> &functions, t_string arith, int importimages,
	int inputfromstdin, t_string inputfilename, std::istream &inputfile, int &errorflag)
{
	if (importimages)
	{
		read_and_prepare_matrix_and_call_skeleton(img, functions, arith,
			inputfromstdin, inputfilename, inputfile, errorflag);
	}

	if (!errorflag)
	{
		image_classification(functions, importimages, inputfromstdin, inputfilename, inputfile, errorflag);
	}
}

int _tmain(int argc, TCHAR *argv[])
{
	int importimages = 1;
	int inputfromstdin = 0;
	int errorflag = 0;
	t_string arith(TEXT("--bigint"));
	t_string inputfilename;

	if (argc == 1)
	{
		print_help();
		return 0;
	}

	for (int i = 1; i < argc; ++i)
	{
		if (!_tcscmp(argv[i], TEXT("--bigint")) || !_tcscmp(argv[i], TEXT("--int"))
			|| !_tcscmp(argv[i], TEXT("--float")) || !_tcscmp(argv[i], TEXT("--rational")))
		{
			arith = argv[i];
		}
		else if (!_tcscmp(argv[i], TEXT("--importimages")))
		{
			importimages = 1;
		}
		else if (!_tcscmp(argv[i], TEXT("--importfunctions")))
		{
			importimages = 0;
		}
		else if (!_tcscmp(argv[i], TEXT("--inputfromstdin")))
			inputfromstdin = 1;
		else if (!_tcscmp(argv[i], TEXT("--noinputfromstdin")))
			inputfromstdin = 0;
		else
		{
			if (_tcslen(argv[i]) >= inputfilename.max_size())
			{
				wcerr << "LinearDiscriminantFunctions Error: name for input file is too long.\n"
					  << "Program terminated." << endl;
				return 1;
			}
			else
			{
				inputfilename = argv[i];
			}
		}
	}

	if (!inputfilename.size() && !inputfromstdin)
	{
		wcerr << "Missing input file. Please enter input file name in command line or use --inputfromstdin option.\n"
			  << "Program terminated." << endl;
		return 1;
	}

	std::ifstream inputfile;

	if (!inputfromstdin)
	{
		inputfile.open(inputfilename, std::ios::in);
		if (inputfile.bad() || !inputfile.is_open())
		{
			wcerr << "LinearDiscriminantFunctions Error: can not open file " << inputfilename << " for input\n"
				  << "                or incorrect option. Program termineted." << endl;
			return 1;
		}
	}

	// Matrix img contains image samples, each of which belongs to its own class
	// Matrix functions contains linear discriminant functions
	if (arith == TEXT("--bigint"))
	{
		matrix<big_int> img, functions;

		handling_matrices(img, functions, arith, importimages,
			inputfromstdin, inputfilename, inputfile, errorflag);
	}
	else if (arith == TEXT("--int"))
	{
		matrix<long> img, functions;
		
		handling_matrices(img, functions, arith, importimages,
			inputfromstdin, inputfilename, inputfile, errorflag);
	}
	else if (arith == TEXT("--float"))
	{
		matrix<double> img, functions;
		
		handling_matrices(img, functions, arith, importimages,
			inputfromstdin, inputfilename, inputfile, errorflag);
	}
	else //rational
	{
		matrix<rational<big_int> > img, functions;
		
		handling_matrices(img, functions, arith, importimages,
			inputfromstdin, inputfilename, inputfile, errorflag);
	}

	inputfile.close();

    return 0;
}

