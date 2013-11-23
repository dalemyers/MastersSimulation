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

#include "Logger.h"

Logger::Logger() {
    myfile.open("log.txt");
}

Logger::~Logger() {
    myfile.close();
}

Logger& Logger::getInstance(){
    static Logger instance;
    return instance;
}


void Logger::fprintf(char* format, ...){
    char* msg;
    va_list args;
    va_start( args, format );
    vasprintf(&msg, format, args );
    va_end( args );

    printf("%s",msg);
    log(msg);

}

void Logger::log(char* s){
    myfile << s;
}
