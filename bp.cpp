
//
// Created by noimo on 14/11/2020.
//
#include "bp_api.h"
#include <vector>
#include <queue>
#include <array>
#include <math.h>
#include <bitset>
#include <vector>
#define UNKNOWN -1


//##################### functions that helps all classes #########################

//given pc and entrance size, calculates the entrance in BTB (entrance size bits
//starting from the second nit of pc
int findEntrance(std::bitset<32> pc, int entranceSize) {
    pc >>= 2;
    int entrance = 0;
    std::bitset<32> tmp;
    tmp.set();
    tmp >>= (32 - entranceSize);
    tmp = tmp & pc;
    entrance = (int)tmp.to_ulong();
    return entrance;
};

//given pc, entrance size and tag size, finds the tag and return it (tag size
// bits after the bits used to calculate the entrance.
std::bitset<32> GetTag(std::bitset<32> pc, int entranceSize,int tagSize) {
    tagSize = (32 - tagSize <= 0) ? (32 - entranceSize - 2) : tagSize;
    pc >>= (entranceSize + 2);
    pc <<= (32 - tagSize);
    pc >>= (32 - tagSize);
    return pc;
}

//given btbSize, calculates entrance size in BTB: 2^entranceSize= btbSize
int findEntranceSize(int btbSize) {
    int counter = 0;
    while (btbSize > 1) {
        btbSize = btbSize / 2;
        counter++;
    }
    return counter;
}

int historyToNumber(int entrance);//declare in order to make things work

//given pc, historySize and history, finds the right fsm entrance and returns it
int getFsmEntrance(int shared, std::bitset<32> pc, int history, int historySize) {
    int fsmMachine;
    switch (shared) {
    case 0: fsmMachine = history;
        break;
    case 1:
        pc >>= 2;
        pc <<= (32 - historySize);
        pc >>= (32 - historySize);
        fsmMachine = (int)pc.to_ulong();
        fsmMachine = fsmMachine ^ history;
        break;
    default: //case 2
        pc >>= 16;
        pc <<= (32 - historySize);
        pc >>= (32 - historySize);
        fsmMachine = (int)pc.to_ulong();
        fsmMachine = fsmMachine ^ history;
        break;
    }
    return fsmMachine;
}


//################ fsm ##################
typedef enum { SNT, WNT, WT, ST } states;

class  fsm {
public:
    states curState;

    fsm(unsigned initFsm); //implementation outside class

    void setState(bool taken) {
        if (taken) {
            switch (curState) {
            case ST: curState = ST;
                break;
            case WT: curState = ST;
                break;
            case WNT: curState = WT;
                break;
            default: curState = WNT; //curState=SNT
                break;
            }
        }
        else {
            switch (curState) {
            case WNT: curState = SNT;
                break;
            case SNT: curState = SNT;
                break;
            case WT: curState = WNT;
                break;
            default: curState = WT;
                break;
            }
        }
    }
};


fsm::fsm(unsigned initFsm) {
    switch (initFsm) {
    case SNT: curState = SNT; break;
    case WNT: curState = WNT; break;
    case WT: curState = WT; break;
    default: curState = ST; break;// case ST:..
    }
}


//##################### Fsm Table for BTB#########################
class FsmTable {
public:
    int historySize;
    bool global;
    int shared;
    int fsmRowNum;
    int fsmColNum;
    int entranceSize;
    std::vector<std::vector<fsm>>* fsmArray;
    int btbSize;
    unsigned initFsmState; //default state for fsm


    FsmTable() = default;

    //constructor
    FsmTable(int historySize, bool global, int shared, int btbSize, unsigned initFsmState) {
        this->historySize = historySize;
        this->global = global;
        this->shared = shared;
        this->entranceSize = findEntranceSize(btbSize);
        this->btbSize = btbSize;
        this->initFsmState = initFsmState;
        if (global) { //either shared or not shared global
            this->fsmRowNum = 1;
        }
        else { //local
            if (shared) {
                this->fsmRowNum = 1;
            }
            this->fsmRowNum = btbSize;
        }
        this->fsmColNum = pow(2, historySize);
        this->fsmArray = new std::vector<std::vector<fsm>>(fsmRowNum, std::vector<fsm>(fsmColNum, WNT)); //TO CHECK
        for (int i = 0; i < fsmRowNum; i++) {
            for (int j = 0; j < fsmColNum; j++) {
                switch (initFsmState) {
                    ;
                case SNT: (*fsmArray)[i][j].curState = SNT; break;
                case WNT: (*fsmArray)[i][j].curState = WNT; break;
                case WT: (*fsmArray)[i][j].curState = WT; break;
                default: (*fsmArray)[i][j].curState = ST; break;// case ST:..
                }
            }
        }
    }

    //
    void insert(std::bitset<32> pc, bool taken, int history, bool PCinSystem, int shared) { //returns true- if predict is currect, false- the predict was wrong

        int entrancePC = findEntrance(pc, this->entranceSize);
        int fsmEntrance = getFsmEntrance(shared, pc, history, historySize);
        if (global) {
            fsmArray->at(0).at(fsmEntrance).setState(taken);
        }
        else {

            if (PCinSystem) {
                fsmArray->at(entrancePC).at(fsmEntrance).setState(taken);
            }
            else { //new PC
                //reset each fsm in the entrance row
                for (int j = 0; j < fsmColNum; j++) {
                    switch (this->initFsmState) {
                    case SNT: fsmArray->at(entrancePC).at(j).curState = SNT; break;
                    case WNT: fsmArray->at(entrancePC).at(j).curState = WNT; break;
                    case WT: fsmArray->at(entrancePC).at(j).curState = WT; break;
                    default: fsmArray->at(entrancePC).at(j).curState = ST; break;// case ST:..
                    }
                }

                fsmArray->at(entrancePC).at(fsmEntrance).setState(taken);
            }
        }
    }

    //given the entrance in BTB, history, shared/not shared and pc, finds the matching
    //fsm and returns it
    fsm* GetFsm(int entrance, int history, int shared, std::bitset<32> pc) { //PC-->after being converted to bits
        int fsmMachine = getFsmEntrance(shared, pc, history, historySize);
        return &fsmArray->at(entrance).at(fsmMachine);
    }
};


//##################### history table for BTB #########################

//a class repesenting each element in history table:
class HistoryElement {
public:
    std::bitset<32> tag;
    uint32_t targetPC;
    std::queue<bool>* history;

    //constructor
    HistoryElement() {
        this->tag = UNKNOWN;
        this->targetPC = UNKNOWN;
        this->history = (new std::queue<bool>());
    }

    HistoryElement(std::bitset<32> tag, uint32_t targetPC, unsigned fsmState) {
        this->tag = tag;
        this->targetPC = targetPC;
        this->history = (new std::queue<bool>());
    }
};

//history table:
class HistoryTable {
public:
    bool global;
    int historySize;
    int btbSize;
    int arraySize;
    int entranceSize;
    std::vector<HistoryElement*>* historyArray;

    HistoryTable() = default;

    HistoryTable(bool global, int historySize, int btbSize) {
        this->global = global;
        this->historySize = historySize;
        this->btbSize = btbSize;
        this->entranceSize = findEntranceSize(btbSize);
        this->arraySize = btbSize;
        historyArray = new std::vector<HistoryElement*>(this->arraySize);
        for (int i = 0; i < arraySize; i++) {
            historyArray->at(i) = new HistoryElement();
        }
        initQueueOfHistoryElement();
    }

    //a function for initializing all history queues
    void initQueueOfHistoryElement() {

        for (int i = 0; i < arraySize; i++) {
            for (int j = 0; j < historySize; j++) {
                std::queue<bool>* history = historyArray->at(i)->history;
                history->push(false);
            }
        }
    }

    // given an entrance, initializes its history queue
    void initQueue(int entrance) {
        while (!(historyArray->at(entrance)->history->empty()))
        {
            historyArray->at(entrance)->history->pop();
        }
        for (int j = 0; j < historySize; j++) {
            std::queue<bool>* history = historyArray->at(entrance)->history;
            history->push(false);
        }
    }

    //given an entrance updates its history queue with taken/not taken
    // (according to given taken parameter)
    void updateQueue(int entrance, bool taken) {
        std::queue<bool>* history = historyArray->at(entrance)->history;
        history->push(taken);
        if (history->size() > historySize)
            history->pop();


    }

    //given an entrance and the tag of the new pc, checks whether this tag represents an existing pc,
    //or it is a new pc/replacement (replacement - when 2 pc mapped to the same entry)
    bool entranceExistsAndPCisTheSame(int entrance, std::bitset<32> new_tag) {
        HistoryElement* element = historyArray->at(entrance);
        if (element->tag == UNKNOWN || element->tag != new_tag) {
            return false;
        }
        return true;
    }

    //given a pc, taken/not taken, target and tag size, inserts the data of the pc
    //to the right place in BTB
    bool insert(std::bitset<32> pc, bool taken, uint32_t targetPC,int tagSize) {
        if (global) {
            updateQueue(0, taken);
            int entrancePC = findEntrance(pc, entranceSize);
            historyArray->at(entrancePC)->targetPC = targetPC; //updating target field of historyElement in entrancePC
            historyArray->at(entrancePC)->tag = GetTag(pc, entranceSize, tagSize);
            return true;
        }
        else { //local
            int entrancePC = findEntrance(pc, entranceSize);
            historyArray->at(entrancePC)->targetPC = targetPC;
            std::bitset<32> new_tag = GetTag(pc, this->entranceSize, tagSize);
            if (entranceExistsAndPCisTheSame(entrancePC, new_tag)) {
                updateQueue(entrancePC, taken);
                return true;
            }
            else {
                //pc is not the original pc in the HistoryElement OR new entrance and new pc
                while (!historyArray->at(entrancePC)->history->empty()) {
                    historyArray->at(entrancePC)->history->pop();
                }
                updateQueue(entrancePC, taken);
                historyArray->at(entrancePC)->tag = GetTag(pc, entranceSize, tagSize);
                //FSM was updated in the BTB class (back to default)
            }
        }

        return false;
    }


    uint32_t getDst(int entrance) {
        return historyArray->at(entrance)->targetPC;
    }
};

//##################### BTB #########################
class BP {
public:
    unsigned int btbSize;
    unsigned  int historySize;
    unsigned  int tagSize;
    bool isGlobalHist;
    bool isGlobalTable;
    int Shared;
    int entranceSize; //how many bits we need to decide the entry in the btb table.
    int branchNum;
    int flushNum;
    HistoryTable* historyTable;
    FsmTable* fsmTable;

    //constructor
    BP(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
        bool isGlobalHist, bool isGlobalTable, int Shared) {
        this->btbSize = btbSize;
        this->historySize = historySize;
        this->tagSize = tagSize;
        this->isGlobalHist = isGlobalHist;
        this->isGlobalTable = isGlobalTable;
        this->Shared = Shared;
        this->historyTable = new HistoryTable(isGlobalHist, historySize, btbSize);
        this->fsmTable = new FsmTable(historySize, isGlobalTable, Shared, btbSize, fsmState); //NETA-fsmInitState is the one the user gives us
        this->branchNum = 0;
        this->flushNum = 0;
    }

};


BP* bp = nullptr;

//##################### implementation of bp_api.h functions #########################
int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
    bool isGlobalHist, bool isGlobalTable, int Shared) {
    try {
        bp = new BP(btbSize, historySize, tagSize, fsmState, isGlobalHist, isGlobalTable, Shared);
    }
    catch (const std::bad_alloc & e) {
        return -1;
    }
    return 0;
}


bool BP_predict(uint32_t pc, uint32_t* dst) {
    std::bitset<32> pcInt(pc);
    int entranceSize = findEntranceSize(bp->btbSize);
    std::bitset<32> tag = GetTag(pcInt, entranceSize, bp->tagSize);
    int entrance = findEntrance(pcInt, entranceSize);
    
    bool isPCinBP = bp->historyTable->entranceExistsAndPCisTheSame(entrance, tag);
    if (bp->fsmTable->global) {
        entrance = 0;
    }

    if (isPCinBP) {
        int history;
        if (bp->historyTable->global) {
            history= historyToNumber(0);
        }
        else {
            history = historyToNumber(findEntrance(pcInt, entranceSize));
        } 
        int shared = bp->Shared;
        states fsmPrediction = bp->fsmTable->GetFsm(entrance, history, shared, pcInt)->curState;
        bool predict = false;
        switch (fsmPrediction) {
        case ST:
        case WT: predict = true;
            break;
        default: predict = false;
            break;
        }
        if (predict) {
            entrance = findEntrance(pcInt, entranceSize);
            *dst = bp->historyTable->getDst(entrance);
            return true;
        }
        else {
            *dst = pc + 4;
            return false;
        }

    }
    else {
        *dst = pc + 4;
        return false;
    }

    return false;


}


//returns the entrance of the specific fsm that match a specific entrance in historyTable
int historyToNumber(int entrance) {
    int binary_history = 0;
    HistoryElement* historyElement = (bp->historyTable->historyArray->at(entrance));
    std::queue<bool> historyQueue = *(historyElement->history);
    while (!historyQueue.empty()) {
        binary_history = (binary_history << 1) + int(historyQueue.front());
        historyQueue.pop();
    }
    return binary_history;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst) {
    std::bitset<32> pcInt(pc);
    int entranceSize = findEntranceSize(bp->btbSize);
    std::bitset<32> tag = GetTag(pcInt, entranceSize,bp->tagSize);
    int entrance = findEntrance(pcInt, entranceSize);
    bool PCinSystem = bp->historyTable->entranceExistsAndPCisTheSame(entrance, tag);
    if (bp->historyTable->global) {
        entrance = 0;
    }
    if (!PCinSystem && !bp->historyTable->global) {
        bp->historyTable->initQueue(entrance);
    }
    int history = historyToNumber(entrance); //in order to find the specific fsm that match to our entrance in historyTable
    (bp->fsmTable->insert(pcInt, taken, history, PCinSystem, bp->Shared));

    if (!((taken&&(pred_dst==targetPc))||(!taken&&(pred_dst==pc+4)))) {
        bp->flushNum++;
    }
    bp->historyTable->insert(pcInt, taken, targetPc, bp->tagSize);
    bp->branchNum++;

    return;
}

void BP_GetStats(SIM_stats* curStats) {
    curStats->br_num = bp->branchNum;
    curStats->flush_num = bp->flushNum;
    if (!bp->isGlobalHist && !bp->isGlobalTable) {
        curStats->size = bp->historyTable->arraySize * (1 + bp->tagSize + 30 + bp->historySize) + bp->fsmTable->fsmRowNum * 2 * bp->fsmTable->fsmColNum;
    }
    if (bp->isGlobalHist && bp->isGlobalTable) {
        //curStats->size = bp->historyTable->arraySize * (1 + bp->tagSize + 30 + bp->historySize) + 2 * bp->fsmTable->fsmColNum;
        curStats->size = bp->historyTable->arraySize * (1 + bp->tagSize + 30) + bp->historySize + 2 * bp->fsmTable->fsmColNum;
    }
    if (!bp->isGlobalHist && bp->isGlobalTable) {
        curStats->size = bp->historyTable->arraySize * (1 + bp->tagSize + 30 + bp->historySize) + 2 * bp->fsmTable->fsmColNum;
    }
    if (bp->isGlobalHist && !bp->isGlobalTable) {
        curStats->size = bp->historyTable->arraySize * (1 + bp->tagSize + 30) + bp->historySize + bp->btbSize * 2 * bp->fsmTable->fsmColNum;
    }
    delete bp;
    return;
}
