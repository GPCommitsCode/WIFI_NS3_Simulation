#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("80211Experiment");

uint64_t lastTotalRxBytes = 0;
void
CalculateThroughput (Ptr<FlowMonitor> monitor)
{
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
  double curTotalRxBytes = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin ();
       i != flowStats.end (); ++i)
    {
      curTotalRxBytes += i->second.rxBytes;
    }
  double curThroughput = (curTotalRxBytes - lastTotalRxBytes) * (double) 8 / 1e5;
  std::cout << Simulator::Now ().GetSeconds () << "s: " << curThroughput << " Mbit/s" << std::endl;
  lastTotalRxBytes = curTotalRxBytes;
}

void
ScheduleNextThroughputSample (Ptr<FlowMonitor> monitor)
{
  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput, monitor);
  Simulator::Schedule (MilliSeconds (100), &ScheduleNextThroughputSample, monitor);
}
int
main (int argc, char *argv[])
{
  uint32_t payloadSize = 1000; // bytes
  uint32_t rtsThresholds[4] = {0, 256, 512, 1000}; // bytes
  uint32_t numNodes = 3;
  double simulationTime = 50.0; // seconds
  double distance = 250.0; // meters

  // Create nodes
  NodeContainer nodes;
  nodes.Create (numNodes);

  // Install Internet stack
  InternetStackHelper internetStack;
  internetStack.Install (nodes);

  // Set mobility model
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator", "MinX", DoubleValue (0.0), "MinY",
                                 DoubleValue (0.0), "DeltaX", DoubleValue (distance), "DeltaY",
                                 DoubleValue (0.0), "GridWidth", UintegerValue (numNodes),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Create wifi devices and set parameters
  // install wireless network models on nodes in a network simulation.
  //wireless standard that operates in the 2.4 GHz frequency band and supports data rates up to 11 Mbps
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  // MAC layer parameters
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  // Physical layer parameters
  YansWifiChannelHelper wifichannel;
  wifichannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifichannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (16.0206));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (16.0206));
  wifiPhy.Set ("TxGain", DoubleValue (0));
  wifiPhy.Set ("RxGain", DoubleValue (0));
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (10.0));
  wifiPhy.Set ("ChannelNumber", UintegerValue (1));
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.SetChannel (wifichannel.Create ());

  // Install wifi devices
  //The Install method returns a NetDeviceContainer object, which contains the network interface cards (i.e., net devices) installed 
  //on each node in the network. The net devices are used to send and receive packets between the nodes in the simulation.
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  //   // Install TCP applications
  uint16_t port = 50000;
  //configured to generate traffic with a specified packet size and data rate.
  OnOffHelper onOffHelper ("ns3::TcpSocketFactory",
                           InetSocketAddress (interfaces.GetAddress (1), port));
                           //Specifically, the traffic will consist of packets with a payload size of
                           // payloadSize bytes, and will be generated at a constant rate of 100 megabits per second
  onOffHelper.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  onOffHelper.SetAttribute ("DataRate", StringValue ("100Mbps"));


  ApplicationContainer appContainer1 = onOffHelper.Install (nodes.Get (0));
  appContainer1.Start (Seconds (1.0 + (double) (rand () % 5)));
  appContainer1.Stop (Seconds (50.0));

  // Install applications on node 2
  ApplicationContainer appContainer2 = onOffHelper.Install (nodes.Get (2));
  appContainer2.Start (Seconds (1.0 + (double) (rand () % 5)));
  appContainer2.Stop (Seconds (50.0));

  //   // Set up TCP source and sink on nodes 0 and 2
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime + 1));

  OnOffHelper onoff ("ns3::TcpSocketFactory", sinkAddress);
  onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  // Install TCP applications on nodes 0 and 2
  ApplicationContainer sourceApps0 = onoff.Install (nodes.Get (0));
  sourceApps0.Start (Seconds (1.0 + (double) (rand () % 5)));
  sourceApps0.Stop (Seconds (simulationTime + 1));

  ApplicationContainer sourceApps2 = onoff.Install (nodes.Get (2));
  sourceApps2.Start (Seconds (1.0 + (double) (rand () % 5)));
  sourceApps2.Stop (Seconds (simulationTime + 1));

  // Enable tracing
  wifiPhy.EnablePcapAll ("wifi-80211");

  // Install FlowMonitor on the network
  // used to install and configure flow monitoring in a simulation. 
  //The InstallAll() method of the FlowMonitorHelper installs a FlowMonitor on all nodes in the simulation.
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
  // Schedule throughput calculation
//  ScheduleNextThroughputSample (monitor);
  // Run the simulation
  Simulator::Stop (Seconds (50.0));
  Simulator::Run ();

  // Print the average bandwidth spent in transmitting RTS, CTS, and ACK
  // flowmon.CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

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
          i->second.rxBytes * 8.0 /
          (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstRxPacket.GetSeconds ());
    }
  double totalTcpSegBandwidth = 0;
  double totalTcpAckBandwidth = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin ();
       i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow (i->first);
      if (tuple.sourceAddress == nodes.Get (0)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal () &&
          tuple.destinationAddress ==
              nodes.Get (1)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ())
        {
          totalTcpSegBandwidth += i->second.txBytes * 8 /
                                  (i->second.timeLastTxPacket.GetSeconds () -
                                   i->second.timeFirstTxPacket.GetSeconds ());
        }
      else if (tuple.sourceAddress ==
                   nodes.Get (2)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal () &&
               tuple.destinationAddress ==
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

out.close();


  //   //Run the simulation with different RTS thresholds f
     for (uint32_t rtsThreshold : rtsThresholds)
      {
        std::cout << "Running simulation with RTS threshold " << rtsThreshold << " bytes"
                 << std::endl;

        // Configure RTS threshold
        WifiMacHelper wifiMac;
       wifiMac.SetType ("ns3::AdhocWifiMac");
     //  wifiMac.SetType ("ns3::NqosWifiMac");
       Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                            UintegerValue (rtsThreshold));

       // Reset statistics
        monitor->SerializeToXmlFile (
            "scratch/80211.flowmon.rts" + std::to_string (rtsThreshold) + ".xml", true, true);
        monitor->CheckForLostPackets ();

  //       // Run the simulation
        Simulator::Stop (Seconds (simulationTime));
         Simulator::Run ();
        Simulator::Destroy ();
   }

  return 0;
}
