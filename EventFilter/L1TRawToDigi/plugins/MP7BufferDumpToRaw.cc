// -*- C++ -*-
//
// Package:    EventFilter/L1TRawToDigi
// Class:      MP7BufferDumpToRaw
// 
/**\class Stage2InputPatternWriter Stage2InputPatternWriter.cc L1Trigger/L1TCalorimeter/plugins/Stage2InputPatternWriter.cc

 Description: [one line class summary]

 Implementation:
     [Notes on implementation]
*/
//
// Original Author:  James Brooke
//         Created:  Tue, 11 Mar 2014 14:55:45 GMT
//
//


// system include files
#include <memory>

// user include files
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

#include "DataFormats/FEDRawData/interface/FEDRawData.h"
#include "DataFormats/FEDRawData/interface/FEDRawDataCollection.h"
#include "DataFormats/FEDRawData/interface/FEDHeader.h"
#include "DataFormats/FEDRawData/interface/FEDTrailer.h"

#include "FWCore/Utilities/interface/CRC16.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <iomanip>
#include <boost/algorithm/string.hpp>

#include "EventFilter/L1TRawToDigi/interface/MP7FileReader.h"
#include "EventFilter/L1TRawToDigi/interface/MP7PacketReader.h"
#include "EventFilter/L1TRawToDigi/interface/Block.h"
#include "EventFilter/L1TRawToDigi/interface/AMCSpec.h"
//#include "EventFilter/L1TRawToDigi/interface/PackingSetup.h"
//
// class declaration
//

namespace l1t {

class MP7BufferDumpToRaw : public edm::EDProducer {
public:
  explicit MP7BufferDumpToRaw(const edm::ParameterSet&);
  ~MP7BufferDumpToRaw();
  
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);
  
  
private:
  virtual void beginJob() override;
  virtual void produce(edm::Event&, const edm::EventSetup&) override;
  virtual void endJob() override;

  std::vector<Block> getBlocks(int iAmc);

  void formatAMC(amc13::Packet& amc13, const std::vector<Block>& blocks, int iAmc);

  void formatRaw(edm::Event& iEvent, amc13::Packet& amc13, FEDRawData& fed_data);
  
  //virtual void beginRun(edm::Run const&, edm::EventSetup const&) override;
  //virtual void endRun(edm::Run const&, edm::EventSetup const&) override;
  //virtual void beginLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&) override;
  //virtual void endLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&) override;
  
  // ----------member data ---------------------------

  // file readers
  MP7FileReader rxFileReader_;
  MP7FileReader txFileReader_;
  unsigned rxIndex_;
  unsigned txIndex_;

  // packet readers
  MP7PacketReader rxPacketReader_;
  MP7PacketReader txPacketReader_;
  //  MP7PacketReader::PacketData::const_iter rxItr_;
  //  MP7PacketReader::PacketData txItr_;

  // formatting parameters
  bool packetisedData_;
 
  // non packetised data parameters
  unsigned nFramesPerEvent_;

  // packetised data parameters

  // hardware params
  unsigned nBoard_;
  unsigned iBoard_;
  std::vector<int> boardId_;

  // board readout params
  std::vector<int> rxBlockLength_;
  std::vector<int> txBlockLength_;  
  bool mux_;
  int muxOffset_;

  // DAQ params
  int fedId_;
  int evType_;
  int fwVer_;
  int slinkHeaderSize_;  // in 8-bit words
  int slinkTrailerSize_;

};

//
// constants, enums and typedefs
//

//
// static data member definitions
//

//
// constructors and destructor
//
  MP7BufferDumpToRaw::MP7BufferDumpToRaw(const edm::ParameterSet& iConfig) :
    rxFileReader_(iConfig.getUntrackedParameter<std::string>("rxFile", "rx_summary.txt")),
    txFileReader_(iConfig.getUntrackedParameter<std::string>("txFile", "tx_summary.txt")),
    rxPacketReader_(iConfig.getUntrackedParameter<std::string>("rxFile", "rx_summary.txt")),
    txPacketReader_(iConfig.getUntrackedParameter<std::string>("txFile", "tx_summary.txt")),
    packetisedData_(iConfig.getUntrackedParameter<bool>("packetisedData", true)),
    nFramesPerEvent_(iConfig.getUntrackedParameter<int>("nFramesPerEvent", 6)),
    iBoard_(iConfig.getUntrackedParameter<int>("boardOffset", 0)),
    boardId_(iConfig.getUntrackedParameter<std::vector<int> >("boardId")),
    rxBlockLength_(iConfig.getUntrackedParameter< std::vector<int> >("rxBlockLength")),
    txBlockLength_(iConfig.getUntrackedParameter< std::vector<int> >("txBlockLength")),
    mux_(iConfig.getUntrackedParameter<bool>("mux", false)),
    fedId_(iConfig.getUntrackedParameter<int>("fedId", 1)),
    evType_(iConfig.getUntrackedParameter<int>("eventType", 1)),
    fwVer_(iConfig.getUntrackedParameter<int>("fwVersion", 1)),
    slinkHeaderSize_(iConfig.getUntrackedParameter<int>("lenSlinkHeader", 16)),
    slinkTrailerSize_(iConfig.getUntrackedParameter<int>("lenSlinkTrailer", 16))
{

  produces<FEDRawDataCollection>();

  // advance pointers for non packetised data
  rxIndex_ = iConfig.getUntrackedParameter<int>("nFramesOffset", 0);
  txIndex_ = rxIndex_ + iConfig.getUntrackedParameter<int>("nFramesLatency", 3);

  LogDebug("L1T") << "Rx index : " << rxIndex_;
  LogDebug("L1T") << "Tx index : " << txIndex_;

  if (rxFileReader_.size() != txFileReader_.size()) {
    edm::LogError("L1T") << "Different number of boards in Rx and Tx files";
  }
  nBoard_ = std::max(rxFileReader_.size(), txFileReader_.size());
  LogDebug("L1T") << "# AMC : " << nBoard_;

  if (nBoard_ != boardId_.size()) {
    edm::LogError("L1T") << "Found " << nBoard_ << " boards, but given " << boardId_.size() << " IDs";
  }

}


MP7BufferDumpToRaw::~MP7BufferDumpToRaw()
{
 
   // do anything here that needs to be done at desctruction time
   // (e.g. close files, deallocate resources etc.)

}


//
// member functions
//

// ------------ method called for each event  ------------
void
MP7BufferDumpToRaw::produce(edm::Event& iEvent, const edm::EventSetup& iSetup)
{
  using namespace edm;

  // AMC 13 packet
  amc13::Packet amc13;    

  // create AMC formatted data
  if (mux_) {
    std::vector<Block> blocks = getBlocks(iBoard_);
    formatAMC(amc13, blocks, boardId_.at(iBoard_));
    iBoard_++;  //advance to next AMC for next event...
    iBoard_ = iBoard_ % nBoard_;
  }
  else {
    for (unsigned iBoard=0; iBoard<nBoard_; ++iBoard) {
      std::vector<Block> blocks = getBlocks(iBoard);
      formatAMC(amc13, blocks, boardId_.at(iBoard));
    }
  }

  LogDebug("L1T") << "AMC13 size " << amc13.size();  

  // prepare the raw data collection
  std::auto_ptr<FEDRawDataCollection> raw_coll(new FEDRawDataCollection());
  FEDRawData& fed_data = raw_coll->FEDData(fedId_);

  formatRaw(iEvent, amc13, fed_data);

  LogDebug("L1T") << "Packing FED ID " << fedId_ << " size " << fed_data.size();
  
  // put the collection in the event
  iEvent.put(raw_coll);  
  
}



std::vector<Block>
MP7BufferDumpToRaw::getBlocks(int iBoard)
{

  LogDebug("L1T") << "Getting blocks from board " << iBoard; 

  std::vector<Block> blocks;

  // Rx blocks first
  for (unsigned link=0; link<rxBlockLength_.size(); ++link) {
    
    unsigned id   = link*2;
    unsigned size = rxBlockLength_.at(link);

    if (size==0) continue;

    std::vector<uint32_t> data;
    if (packetisedData_) {

      const PacketData& p = rxPacketReader_.get(iBoard);
      PacketData::const_iterator itr = p.begin();
      for (unsigned i=0; i<rxIndex_; i++) itr++;

      LogDebug("L1T") << "Found packet [" << itr->first_ << ", " << itr->last_ << "]";
      LogDebug("L1T") << "Link " << link << " has " << itr->links_.find(link)->second.size() << " frames";

      for (unsigned iFrame=0; iFrame<itr->links_.find(link)->second.size(); ++iFrame) {
	uint64_t d = itr->links_.find(link)->second.at(iFrame);
      	data.push_back(d);
      }
    }
    else {
      for (unsigned iFrame=rxIndex_; iFrame<rxIndex_+size; ++iFrame) {
	uint64_t d = rxFileReader_.get(iBoard).link(link).at(iFrame);
	//	LogDebug("L1T") << "Frame " << iFrame << " : " << std::hex << d;
	if ((d & 0x100000000) > 0) data.push_back( d & 0xffffffff );
      }
      rxIndex_ += nFramesPerEvent_;
    }
    
    LogDebug("L1T") << "AMC " << iBoard << " block " << id << ", size " << data.size();

    Block block(id, data);
    blocks.push_back(block);
    
  }
  
  // then Tx blocks
  for (unsigned link=0; link<txBlockLength_.size(); ++link) {
    
    unsigned id   = (link*2)+1;
    unsigned size = txBlockLength_.at(link);

    if (size==0) continue;

    std::vector<uint32_t> data;
    if (packetisedData_) {
      const PacketData& p = txPacketReader_.get(iBoard);
      PacketData::const_iterator itr = p.begin();
      for (unsigned i=0; i<txIndex_; i++) itr++;
      for (unsigned iFrame=itr->first_; iFrame<itr->last_; ++iFrame) {
	uint64_t d = itr->links_.find(link)->second.at(iFrame);
	data.push_back(d);
      }
    }
    else {
      for (unsigned iFrame=txIndex_; iFrame<txIndex_+size; ++iFrame) {
	uint64_t d = txFileReader_.get(iBoard).link(link).at(iFrame);
	//	LogDebug("L1T") << "Frame " << iFrame << " : " << std::hex << d;
	if ((d & 0x100000000) > 0) data.push_back( d & 0xffffffff );
      }
      txIndex_ += nFramesPerEvent_;
    }
    
    LogDebug("L1T") << "AMC " << iBoard << " block " << id << ", size " << data.size();

    Block block(id, data);

    blocks.push_back(block);




  }

  LogDebug("L1T") << "AMC " << iBoard << ", read " << blocks.size() << " blocks";

  return blocks;
  
}    


void
MP7BufferDumpToRaw::formatAMC(amc13::Packet& amc13, const std::vector<Block>& blocks, int iAmc) {

  LogDebug("L1T") << "Formatting AMC " << iAmc;
  
  std::vector<uint32_t> load32;
  // TODO this is an empty word to be replaced with a proper MP7
  // header containing at least the firmware version
  load32.push_back(0);
  for (const auto& block: blocks) {
    LogDebug("L1T") << "Adding block " << block.header().getID() << " with size " << block.payload().size();
    auto load = block.payload();
    
#ifdef EDM_ML_DEBUG
    std::stringstream s("");
    s << "Block content:" << std::endl << std::hex << std::setfill('0');
    for (const auto& word: load)
      s << std::setw(8) << word << std::endl;
    LogDebug("L1T") << s.str();
#endif
    
    load32.push_back(block.header().raw());
    load32.insert(load32.end(), load.begin(), load.end());
  }
  
  LogDebug("L1T") << "Converting payload " << iAmc;
  
  std::vector<uint64_t> load64;
  for (unsigned int i = 0; i < load32.size(); i += 2) {
    uint64_t word = load32[i];
    if (i + 1 < load32.size())
      word |= static_cast<uint64_t>(load32[i + 1]) << 32;
    load64.push_back(word);
  }
  
  LogDebug("L1T") << "Creating AMC packet " << iAmc;
  
  amc13.add(iAmc, load64);

}

  
  
void
MP7BufferDumpToRaw::formatRaw(edm::Event& iEvent, amc13::Packet& amc13, FEDRawData& fed_data)
{

  unsigned int size = slinkHeaderSize_ + slinkTrailerSize_ + amc13.size() * 8;
  fed_data.resize(size);
  unsigned char * payload = fed_data.data();
  unsigned char * payload_start = payload;

  auto bxId = iEvent.bunchCrossing();
  auto evtId = iEvent.id().event();

  LogDebug("L1T") << "Creating FEDRawData ID " << fedId_ << ", size " << size;

  FEDHeader header(payload);
  header.set(payload, evType_, evtId, bxId, fedId_);

  payload += slinkHeaderSize_;

  amc13.write(iEvent, payload, size - slinkHeaderSize_ - slinkTrailerSize_);

  payload += amc13.size() * 8;

  FEDTrailer trailer(payload);
  trailer.set(payload, size / 8, evf::compute_crc(payload_start, size), 0, 0);

}


// ------------ method called once each job just before starting event loop  ------------
void 
MP7BufferDumpToRaw::beginJob()
{



}


// ------------ method called once each job just after ending the event loop  ------------
void 
MP7BufferDumpToRaw::endJob() 
{


}

// ------------ method called when starting to processes a run  ------------
/*
void 
MP7BufferDumpToRaw::beginRun(edm::Run const&, edm::EventSetup const&)
{
}
*/

// ------------ method called when ending the processing of a run  ------------
/*
void 
MP7BufferDumpToRaw::endRun(edm::Run const&, edm::EventSetup const&)
{
}
*/

// ------------ method called when starting to processes a luminosity block  ------------
/*
vvoid 
MP7BufferDumpToRaw::beginLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&)
{
}
*/

// ------------ method called when ending the processing of a luminosity block  ------------
/*
void 
MP7BufferDumpToRaw::endLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&)
{
}
*/

// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void
MP7BufferDumpToRaw::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  //The following says we do not know what parameters are allowed so do no validation
  // Please change this to state exactly what you do use, even if it is no parameters
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault(desc);
}

}

using namespace l1t;
//define this as a plug-in
DEFINE_FWK_MODULE(MP7BufferDumpToRaw);
