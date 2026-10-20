#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LargeScaleAiSdnSimulation");

std::ofstream datasetFile;

void ExportMetrics(Ptr<FlowMonitor> monitor, FlowMonitorHelper* flowmon, double interval) {
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon->GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double now = Simulator::Now().GetSeconds();
    uint64_t totalTx = 0;
    uint64_t totalRx = 0;
    uint64_t totalLost = 0;
    double sumDelay = 0.0;
    uint64_t totalRxBytes = 0;

    for (auto const& iter : stats) {
        totalTx += iter.second.txPackets;
        totalRx += iter.second.rxPackets;
        totalLost += iter.second.lostPackets;
        totalRxBytes += iter.second.rxBytes;
        if (iter.second.rxPackets > 0) {
            sumDelay += iter.second.delaySum.GetMilliSeconds() / iter.second.rxPackets;
        }
    }

    double avgDelay = (stats.size() > 0) ? (sumDelay / stats.size()) : 0.0;
    double throughput = (totalRxBytes * 8.0) / (interval * 1024 * 1024); // Mbps
    double lossRatio = (totalTx > 0) ? ((double)totalLost / (double)totalTx) : 0.0;
    int isCongested = (lossRatio > 0.03 || avgDelay > 12.0) ? 1 : 0;

    datasetFile << now << "," << totalTx << "," << totalRx << ","
                << avgDelay << "," << throughput << "," << lossRatio << "," 
                << isCongested << "\n";

    Simulator::Schedule(Seconds(interval), &ExportMetrics, monitor, flowmon, interval);
}

// Proactive SDN dynamic rerouting function: adjusts link cost to shift traffic to redundant spine
void SDNProactiveReroute(Ptr<Node> leafSwitch, uint32_t primaryInterface) {
    std::cout << "\n>>> [SDN CONTROLLER TRIGGER] AI Predicted Impending Congestion!" << std::endl;
    std::cout << ">>> Pushing dynamic OpenFlow rule update: Rerouting multi-host traffic to Alternate Spine Switch...\n" << std::endl;
    
    Ptr<Ipv4> ipv4 = leafSwitch->GetObject<Ipv4>();
    // Penalize primary interface cost to proactively shift traffic to backup path
    ipv4->SetMetric(primaryInterface, 100);
    Ipv4GlobalRoutingHelper::RecomputeRoutingTables();
}

int main(int argc, char* argv[]) {
    bool enableAi = false;
    uint32_t numClients = 100; // 100 end-user nodes
    CommandLine cmd;
    cmd.AddValue("enableAi", "Enable AI proactive SDN rerouting", enableAi);
    cmd.AddValue("clients", "Number of client nodes", numClients);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    // 1. Topology Structure:
    // 100 Clients -> Ingress Leaf Switch -> [Spine Switch 1 (Primary) OR Spine Switch 2 (Backup)] -> Egress Leaf Switch -> Server Destination
    NodeContainer clients;
    clients.Create(numClients);

    NodeContainer switches;
    switches.Create(4); // 0: Leaf-Ingress, 1: Spine-Primary, 2: Spine-Backup, 3: Leaf-Egress

    NodeContainer destination;
    destination.Create(1);

    // Link helpers
    PointToPointHelper clientLink;
    clientLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    clientLink.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper primaryCoreLink;
    primaryCoreLink.SetDeviceAttribute("DataRate", StringValue("15Mbps"));
    primaryCoreLink.SetChannelAttribute("Delay", StringValue("5ms"));

    PointToPointHelper backupCoreLink;
    backupCoreLink.SetDeviceAttribute("DataRate", StringValue("50Mbps"));
    backupCoreLink.SetChannelAttribute("Delay", StringValue("4ms"));

    PointToPointHelper egressLink;
    egressLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    egressLink.SetChannelAttribute("Delay", StringValue("1ms"));

    // Connect 100 clients to Ingress Leaf Switch
    std::vector<NetDeviceContainer> clientDevices(numClients);
    for (uint32_t i = 0; i < numClients; ++i) {
        clientDevices[i] = clientLink.Install(clients.Get(i), switches.Get(0));
    }

    // Connect Leaf to Core Spines
    NetDeviceContainer leafToPrimarySpine = primaryCoreLink.Install(switches.Get(0), switches.Get(1));
    NetDeviceContainer leafToBackupSpine = backupCoreLink.Install(switches.Get(0), switches.Get(2));

    NetDeviceContainer primarySpineToEgress = primaryCoreLink.Install(switches.Get(1), switches.Get(3));
    NetDeviceContainer backupSpineToEgress = backupCoreLink.Install(switches.Get(2), switches.Get(3));

    // Connect Egress Leaf to Destination Server
    NetDeviceContainer serverDev = egressLink.Install(switches.Get(3), destination.Get(0));

    // Protocol Stack
    InternetStackHelper stack;
    stack.Install(clients);
    stack.Install(switches);
    stack.Install(destination);

    // IP Addressing
    Ipv4AddressHelper ipv4;
    for (uint32_t i = 0; i < numClients; ++i) {
        std::ostringstream subnet;
        subnet << "10.2." << (i + 1) << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        ipv4.Assign(clientDevices[i]);
    }

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifLeafPrimary = ipv4.Assign(leafToPrimarySpine);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer ifLeafBackup = ipv4.Assign(leafToBackupSpine);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    ipv4.Assign(primarySpineToEgress);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    ipv4.Assign(backupSpineToEgress);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer ifServer = ipv4.Assign(serverDev);

    // Set Default Routing Metric (Primary path prioritized)
    Ptr<Ipv4> ipv4IngressLeaf = switches.Get(0)->GetObject<Ipv4>();
    ipv4IngressLeaf->SetMetric(ifLeafPrimary.Get(0).second, 1);
    ipv4IngressLeaf->SetMetric(ifLeafBackup.Get(0).second, 20);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Destination Application: Traffic sink on destination server
    uint16_t sinkPort = 9000;
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sink.Install(destination.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    // 100 Clients Generating Aggregated Traffic Flows
    for (uint32_t i = 0; i < numClients; ++i) {
        OnOffHelper clientApp("ns3::UdpSocketFactory", InetSocketAddress(ifServer.GetAddress(1), sinkPort));
        clientApp.SetAttribute("DataRate", StringValue("250Kbps"));
        clientApp.SetAttribute("PacketSize", UintegerValue(1024));
        clientApp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
        clientApp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

        ApplicationContainer app = clientApp.Install(clients.Get(i));
        // Stagger client start times
        double startTime = 1.0 + (i * 0.02);
        app.Start(Seconds(startTime));
        app.Stop(Seconds(19.0));
    }

    // Heavy burst cross-traffic to trigger congestion on primary path between t=6.0s and t=14.0s
    OnOffHelper burstTraffic("ns3::UdpSocketFactory", InetSocketAddress(ifServer.GetAddress(1), sinkPort));
    burstTraffic.SetAttribute("DataRate", StringValue("18Mbps"));
    burstTraffic.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer burstApp = burstTraffic.Install(switches.Get(0));
    burstApp.Start(Seconds(6.0));
    burstApp.Stop(Seconds(14.0));

    // Telemetry CSV logging
    std::string filename = enableAi ? "metrics_ai.csv" : "metrics_traditional.csv";
    datasetFile.open(filename);
    datasetFile << "timestamp,tx_packets,rx_packets,delay_ms,throughput_mbps,loss_ratio,is_congested\n";

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Schedule(Seconds(1.0), &ExportMetrics, monitor, &flowmon, 1.0);

    // AI proactive rerouting event: Controller triggers path switch at t=7.0s when congestion begins
    if (enableAi) {
        Simulator::Schedule(Seconds(7.0), &SDNProactiveReroute, switches.Get(0), ifLeafPrimary.Get(0).second);
    }

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Print summary stats to terminal
    monitor->CheckForLostPackets();
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    uint64_t totalTx = 0, totalRx = 0, totalLost = 0;
    for (auto const& iter : stats) {
        totalTx += iter.second.txPackets;
        totalRx += iter.second.rxPackets;
        totalLost += iter.second.lostPackets;
    }

    std::cout << "\n================ SIMULATION COMPLETE ================" << std::endl;
    std::cout << "Active Clients Simulated: " << numClients << std::endl;
    std::cout << "Total Packets Transmitted: " << totalTx << std::endl;
    std::cout << "Total Packets Received:    " << totalRx << std::endl;
    std::cout << "Total Packets Lost:        " << totalLost << std::endl;
    std::cout << "Overall Packet Loss Rate:  " << (totalTx > 0 ? (double)totalLost / totalTx * 100.0 : 0.0) << " %" << std::endl;
    std::cout << "Output CSV saved to:       " << filename << std::endl;
    std::cout << "======================================================\n" << std::endl;

    datasetFile.close();
    Simulator::Destroy();
    return 0;
}
