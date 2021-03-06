/*
 * Copyright (C) 2019  SCS Lab <scs-help@cs.iit.edu>, Hariharan
 * Devarajan <hdevarajan@hawk.iit.edu>, Xian-He Sun <sun@iit.edu>
 *
 * This file is part of HFetch
 * 
 * HFetch is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */
//
// Created by hariharan on 3/19/19.
//

#ifndef HFETCH_DPE_FACTORY_H
#define HFETCH_DPE_FACTORY_H

#include <src/common/singleton.h>
#include "dpe.h"
#include "max_bw_dpe.h"

class DPEFactory{
public:
    DPEFactory(){}
    std::shared_ptr<DPE> GetEngine(DataPlacementEngineType type){
        AutoTrace trace = AutoTrace("DPEFactory::GetEngine",type);
        switch (type){
            case DataPlacementEngineType::MAX_BW:{
                return Singleton<MaxBandwidthDPE>::GetInstance();
            };
        }
    }
};
#endif //HFETCH_DPE_FACTORY_H
