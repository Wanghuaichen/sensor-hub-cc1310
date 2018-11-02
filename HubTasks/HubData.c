/*
 * HubData.c
 *
 *  Created on: Jul 5, 2018
 *      Author: Dillon
 */

#include "HubData.h"

sensorNode nodes[MAX_NODES] = {0};
uint8_t numNodes = 0;

uint8_t getNodeRef(uint8_t addrN[])
{
    uint8_t i;
    uint8_t nodeCnt = 1;

    for(i = 0; i < MAX_NODES; i++){
        if(nodes[i].set){
            nodeCnt++;
            if(memcmp(addrN, nodes[i].addrA, 8) == 0){
                break;
            }
        }
        if(nodeCnt > numNodes){//all available nodes read
            return (MAX_NODES + 1);
        }
    }
    return i;
}

uint16_t getValidRefs(char refs[])
{
    refs[0] = '\0';
    //check if any nodes are set
    if(numNodes == 0){
        return 0;
    }

    uint8_t i, rcnt = 0;
    char rtempS[] = "###,";
    for(i = 0; i < MAX_NODES; i++){
        if(nodes[i].set){
            System_sprintf(rtempS, "%d,", i);
            strcat(refs, rtempS);//add to ref string
            rcnt++;
        }
    }

    i = strlen(refs);

    refs[--i] = '\0';//remove last comma

    return rcnt;//return number of references
}

bool checkIfAdded(uint8_t addr[], uint8_t *ref)
{
    uint8_t i;
    for(i = 0; i < MAX_NODES; i++){
        if(nodes[i].set){
            if(memcmp(addr, nodes[i].addrA, 8) == 0){
                *ref = i;
                return true;
            }
        }
    }
    return false;
}
