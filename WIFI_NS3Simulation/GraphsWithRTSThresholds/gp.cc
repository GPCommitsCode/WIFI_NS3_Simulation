#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor.h"
#include "ns3/internet-module.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/mobility-module.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/tcp-westwood.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/simulator.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace ns3;
using namespace std;

Ptr<PacketSink> sink; /* Pointer to the packet sink application */

void
SetDefault (uint32_t payload)
{
  string RtsCtsThreshold = "1000";
  string FragmentationThreshold = "1000";
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                      StringValue (RtsCtsThreshold));
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold",
                      StringValue (FragmentationThreshold));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payload));
}

void
setTCPVariant (string tcpVariant)
{
  // return;
  if (tcpVariant.compare ("ns3::TcpWestwood") == 0)
    {
      // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                          TypeIdValue (TcpWestwood::GetTypeId ()));
      // the default protocol type in ns3::TcpWestwood is WESTWOOD
      Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOOD));
    }
  else if (tcpVariant.compare ("ns3::TcpHybla") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpHybla::GetTypeId ()));
    }
  else
    {
      TypeId tcpTid;
      NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (tcpVariant, &tcpTid),
                           "TypeId " << tcpVariant << " not found");
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                          TypeIdValue (TypeId::LookupByName (tcpVariant)));
    }
}

int
GenerateRandomNumber ()
{
  std::srand (time (NULL));
  int x = (std::rand () % 5);
  return x + 1;
}
void
CalculateThroughput ()
{
  static uint64_t lastTotalRx = 0;
  Time now = Simulator::Now (); /* Return the simulator's virtual time. */
  double cur = (sink->GetTotalRx () - lastTotalRx) * (double) 8 /
               1e5; /* Convert Application RX Packets to MBits. */
  std::cout << now.GetSeconds () << "s: \t" << cur << " Mbit/s" << std::endl;
  lastTotalRx = sink->GetTotalRx ();
  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
}

int
main (int argc, char *argv[])
{
  uint32_t payload = 1000; // default 1000bytes in the question
  string datarate = "100"; // default taken as 100Mbps
  string tcpVariant = "TcpWestwood";
  string phyRate = "HtMcs7";

  bool pcapTracing = true; // PCAP Tracing is enabled or not
  //uint32_t payloadSize = 1000; // bytes
 // uint32_t rtsThresholds[4] = {0, 256, 512, 1000}; // bytes
  uint32_t numNodes = 3;
  double simulationTime = 50.0; // seconds

  SetDefault (payload);

  datarate = datarate + std::string ("Mbps");
  tcpVariant = std::string ("ns3::") + tcpVariant;

  setTCPVariant (tcpVariant);

  cout << "DataRate is:: " << datarate << ",PayLoad is:: " << payload << "bytes and"
       << " TcpVariant is:: " << tcpVariant << "\n";

  WifiMacHelper mac;

  WifiHelper wifiHelper;

  wifiHelper.SetStandard (ns3::WIFI_STANDARD_80211a);

  // set Legacy Channel
  YansWifiChannelHelper wifichannel;
  wifichannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifichannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");

  // set up physical layer
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel (wifichannel.Create ());

  NodeContainer apNode, staWifiNode1, staWifiNode2;
  staWifiNode1.Create (1);
  apNode.Create (1);
  staWifiNode2.Create (1);
  Ptr<Node> apWifiNode = apNode.Get (0);
  NodeContainer staWifiNodes;
  for (int ii = 0; ii < 2; ii++)
    {
      if (ii == 0)
        staWifiNodes.Add (staWifiNode1);
      else if (ii == 1)
        staWifiNodes.Add (staWifiNode2);
    }
  Ssid ssid = Ssid ("network");
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice;
  apDevice = wifiHelper.Install (wifiPhy, mac, apWifiNode);

  // Configure STA
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid));

  NetDeviceContainer staDevices;
  staDevices = wifiHelper.Install (wifiPhy, mac, staWifiNodes);

  // Mobility model
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> position = CreateObject<ListPositionAllocator> ();
  position->Add (Vector (5.0, 0.0, 0.0));
  position->Add (Vector (5.0, 250.0, 0.0));
  position->Add (Vector (5.0, 500.0, 0.0));
  mobility.SetPositionAllocator (position);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  int j = 0;
  NodeContainer::Iterator i;
  std::cout << "Nodes related to mobility in order are:: \n";

  for (i = staWifiNodes.Begin (); i != staWifiNodes.End (); ++i)
    {
      mobility.Install ((*i));
      std::cout << (*i) << " ";
      if (j == 0)
        {
          mobility.Install (apWifiNode);
          std::cout << apWifiNode << " ";
          j++;
        }
      else if (j == 1)
        {
          std::cout << "\n";
        }
    }

  // Internet stack
  InternetStackHelper stack;
  NodeContainer nodes;
  j = 0;
  std::cout << "Nodes related to stack in order are:: ";
  for (i = staWifiNodes.Begin (); i != staWifiNodes.End (); ++i)
    {
      nodes.Add ((*i));
      std::cout << (*i) << " ";
      if (j == 0)
        {
          nodes.Add (apWifiNode);
          std::cout << apWifiNode << " ";
          j++;
        }
      else if (j == 1)
        std::cout << "\n";
    }
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface, staInterface;
  apInterface = address.Assign (apDevice);
  staInterface = address.Assign (staDevices);
  // Populate routing table
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install TCP Receiver on the access point
  uint16_t portno = 9;
  Ipv4Address ipv4 = Ipv4Address::GetAny ();
  std::string protocol = "ns3::TcpSocketFactory";
  Address inetaddress = Address (InetSocketAddress (ipv4, portno));
  PacketSinkHelper sinkHelper (protocol, inetaddress);
  ApplicationContainer sinkApp = sinkHelper.Install (apWifiNode);
  sink = StaticCast<PacketSink> (sinkApp.Get (0));

  //---

  // Install TCP Transmitter on the station
  ipv4 = apInterface.GetAddress (0);
  inetaddress = Address (InetSocketAddress (ipv4, portno));
  OnOffHelper server (protocol, inetaddress);
  server.SetAttribute ("PacketSize", UintegerValue (payload));
  server.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  server.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  server.SetAttribute ("DataRate", DataRateValue (DataRate (datarate)));
  ApplicationContainer serverApp = server.Install (staWifiNodes);

  //record traffic
  // Flow monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll ();

  // Start Applications in random (0,5) create tcp connection between node 1,0/node 2,1
  sinkApp.Start (Seconds (0.0));
  int x = GenerateRandomNumber ();
  std::cout << "Hello!! RandomNumber Generated is:: " << x << "\n";
  double y = (double) x * 1.0;
  serverApp.Start (Seconds (y));
  Simulator::Schedule (Seconds (y + 0.1), &CalculateThroughput);

  // Enable Traces
  if (pcapTracing)
    {
      wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
      wifiPhy.EnablePcap ("AccessPoint", apDevice);
      wifiPhy.EnablePcap ("Station", staDevices);
    }

  Simulator::Stop (Seconds (50.0));
  Simulator::Run ();

  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin ();
       i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress
                << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Tx Bytes: " << i->second.txBytes << "\n";
      std::cout << "  Tx Offered: " << i->second.txBytes * 8.0 / 50.0 / 1000000 << " Mbps\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Rx Bytes: " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 50.0 / 1000000 << " Mbps\n";
    }
  double totalRtsBandwidth = 0.0;
  double totalCtsBandwidth = 0.0;
  double totalAckBandwidth = 0.0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin ();
       i != stats.end (); ++i)
    {

      Ipv4FlowClassifier::FiveTuple fiveTuple = classifier->FindFlow (i->first);

      if (fiveTuple.sourceAddress !=
              nodes.Get (1)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal () &&
          fiveTuple.destinationAddress !=
              nodes.Get (1)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ())
        {
          continue;
        }
      totalRtsBandwidth +=
          i->second.txBytes * 8.0 /
          (i->second.timeLastTxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ());
      totalCtsBandwidth +=
          i->second.rxBytes * 8.0 /
          (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstRxPacket.GetSeconds ());
      totalAckBandwidth +=
          i->second.txBytes * 8.0 /
          (i->second.timeLastTxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ());
    }
  double totalTcpSegBandwidth = 0;
  double totalTcpAckBandwidth = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin ();
       i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow (i->first);
      if (tuple.sourceAddress == nodes.Get (0)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal () &&
          tuple.destinationAddress ==
              nodes.Get (1)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal () || tuple.sourceAddress ==
                   nodes.Get (2)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal () &&
               tuple.destinationAddress ==
                   nodes.Get (1)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ())
        {
          totalTcpSegBandwidth += i->second.txBytes * 8 /
                                  (i->second.timeLastTxPacket.GetSeconds () -
                                   i->second.timeFirstTxPacket.GetSeconds ());
        }
      else if (tuple.sourceAddress ==
                   nodes.Get (1)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ())
          
        {
          totalTcpAckBandwidth += i->second.txBytes * 8 /
                                  (i->second.timeLastTxPacket.GetSeconds () -
                                   i->second.timeFirstTxPacket.GetSeconds ());
        }
    }
  uint64_t totalCollisionBytes = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin ();
       i != stats.end (); ++i)
    {
      totalCollisionBytes += i->second.txBytes - i->second.rxBytes - i->second.lostPackets * 1000;
    }
  // Calculate average bandwidth spent in transmitting RTS, CTS, and ACK
  double avgRtsBandwidth = totalRtsBandwidth / (simulationTime * numNodes);
  double avgCtsBandwidth = totalCtsBandwidth / (simulationTime * numNodes);
  double avgAckBandwidth = totalAckBandwidth / (simulationTime * numNodes);

  // Calculate average bandwidth spent in transmitting TCP segments and TCP acks
  double avgTcpSegBandwidth = totalTcpSegBandwidth / (simulationTime * numNodes);
  double avgTcpAckBandwidth = totalTcpAckBandwidth / (simulationTime * numNodes);

  // Get the average bandwidth wasted due to collisions
  double avgCollisionBandwidth = totalCollisionBytes / 50.0 / 3.0 / 1000.0;

  // Print the results
  std::cout << "Average bandwidth spent on RTS: " << avgRtsBandwidth << " Kbps" << std::endl;
  std::cout << "Average bandwidth spent on CTS: " << avgCtsBandwidth << " Kbps" << std::endl;
  std::cout << "Average bandwidth spent on ACK: " << avgAckBandwidth << " Kbps" << std::endl;
  std::cout << "Average bandwidth spent on TCP segments: " << avgTcpSegBandwidth << " Kbps"
            << std::endl;
  std::cout << "Average bandwidth spent on TCP ACKs: " << avgTcpAckBandwidth << " Kbps"
            << std::endl;
  std::cout << "Average bandwidth wasted due to collisions: " << avgCollisionBandwidth << " Kbps"
            << std::endl;

  double totalTxBytes = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin ();
       i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.protocol == 6) // TCP
        {
          totalTxBytes += i->second.txBytes;
        }
    }

  std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMonitor->GetFlowStats ();
  for (uint32_t i = 0; i < nodes.GetN (); i++)
    {
      Ptr<Node> node = nodes.Get (i);
      Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
      Ipv4InterfaceAddress ifaceAddr = ipv4->GetAddress (1, 0);
      Ipv4Address addr = ifaceAddr.GetLocal ();

      double totalTxBytes = 0.0;
      double startTime = 0.0;
      double endTime = 50.0;

      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator it = stats.begin ();
           it != stats.end (); ++it)
        {
          //Ipv4FlowClassifier::FiveTuple tuple = classifier.FindFlow (it->first);
          Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow (it->first);

          if (tuple.sourceAddress == addr)
            {
              totalTxBytes += it->second.txBytes;
              startTime = std::min (startTime, it->second.timeFirstTxPacket.GetSeconds ());
              endTime = std::max (endTime, it->second.timeLastTxPacket.GetSeconds ());
            }
        }

      double tcpThroughput = totalTxBytes * 8 / (endTime - startTime) / 1e6;
      std::cout << "Node " << i << " TCP throughput: " << tcpThroughput << " Mbps" << std::endl;
    }

   Simulator::Destroy ();
   double averageThroughput = ((sink->GetTotalRx () * 8) / (1e6 * simulationTime));
   std::cout << "\nAverage throughput: " << averageThroughput << " Mbit/s" << std::endl;
  
  return 0;
}
