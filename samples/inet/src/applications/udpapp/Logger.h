//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#ifndef LOGGER_H_
#define LOGGER_H_

#include <iostream>
#include <fstream>
#include <stdarg.h>
#include <stdio.h>

class Logger
{
    public:
        static Logger& getInstance();
        void log(char* s);
        void fprintf(char* s, ...);
    private:
        std::ofstream myfile;
        Logger();                   // Constructor? (the {} brackets) are needed here.
        ~Logger();
        // Dont forget to declare these two. You want to make sure they
        // are unaccessable otherwise you may accidently get copies of
        // your singleton appearing.
        Logger(Logger const&);              // Don't Implement
        void operator=(Logger const&); // Don't implement
};

#endif /* LOGGER_H_ */



