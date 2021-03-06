/*
 * l_nvm.h
 *
 *  Created on: 18 feb. 2019
 *      Author: pablo
 */

#ifndef SRC_PROTOP_LIBS_L_NVM_L_NVM_H_
#define SRC_PROTOP_LIBS_L_NVM_L_NVM_H_

#include "../../PROTOP_libs/l_nvm/nvm.h"

struct nvm_device_serial xmega_id;

char *NVMEE_readID( void );
void NVMEE_test_read( char *addr, char *size );
void NVMEE_test_write( char *addr, char *str );


#endif /* SRC_PROTOP_LIBS_L_NVM_L_NVM_H_ */
