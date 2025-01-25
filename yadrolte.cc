#include <iostream>
#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/mobility-module.h>
#include <ns3/lte-module.h>
#include <ns3/internet-module.h>
#include <ns3/applications-module.h>
#include "ns3/point-to-point-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("YADROLTE");
int main(int argc, char *argv[]) {
	//Setting the full-buffer traffic mode
	Config::SetDefault("ns3::UdpClient::Interval", TimeValue(MilliSeconds(1)));
	Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(1000000));
	Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(10 * 1024));
	Time simTime = MilliSeconds(1100);
	Time interPacketInterval = MilliSeconds(1);
	Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
	lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");
	Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
	lteHelper->SetEpcHelper(epcHelper);
	
	Ptr<Node> pgw = epcHelper->GetPgwNode();

	//Creating a single RemoteHost
	NodeContainer remoteHostContainer;
	remoteHostContainer.Create(1);
	Ptr<Node> remoteHost = remoteHostContainer.Get(0);
	InternetStackHelper internet;
	internet.Install(remoteHostContainer);
	
	//Create the Internet
	PointToPointHelper p2ph;
	p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
	p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
	p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
	NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
	Ipv4AddressHelper ipv4h;
	ipv4h.SetBase("1.0.0.0", "255.0.0.0");
	Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
	//Interface 0 is localhost's packet gateway, 1 is the p2p device
	Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

	Ipv4StaticRoutingHelper ipv4RoutingHelper;
	Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
	ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
	remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
	//Creating nodes and setting mobility
	NodeContainer ueNodes;
	NodeContainer enbNodes;
	enbNodes.Create(1);
	ueNodes.Create(2);
	MobilityHelper mobility;
	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	mobility.Install(enbNodes);
	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	mobility.Install(ueNodes);
	//Installing LTE Devices on the nodes
	NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
	NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);
	//Installing the IP stack on the UEs
	 internet.Install(ueNodes);
	Ipv4InterfaceContainer ueIpIface;
	ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
	//Assigning IP address to UEs, and install applications
	for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
		Ptr<Node> ueNode = ueNodes.Get(u);
		//Setting the default gateway for the UE
		Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
		ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
	}
	// Attaching UEs to eNodeB
	for (uint16_t i = 0; i < 2; i++) {
		lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
	}

	//Installing and starting applications on UEs and remote host
	uint16_t dlPort = 1100;
	uint16_t ulPort = 2000;
	ApplicationContainer clientApps;
	ApplicationContainer serverApps;
	for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
	{
		++dlPort;
		PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));
		serverApps.Add(dlPacketSinkHelper.Install(ueNodes.Get(u)));
		UdpClientHelper dlClient(ueIpIface.GetAddress(u), dlPort);
		dlClient.SetAttribute("Interval", TimeValue(interPacketInterval));
		dlClient.SetAttribute("MaxPackets", UintegerValue(1000000));
		clientApps.Add(dlClient.Install(remoteHost));
		++ulPort;
		PacketSinkHelper ulPacketSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), ulPort));
		serverApps.Add(ulPacketSinkHelper.Install(remoteHost));
		UdpClientHelper ulClient(remoteHostAddr, ulPort);
		ulClient.SetAttribute("Interval", TimeValue(interPacketInterval));
		ulClient.SetAttribute("MaxPackets", UintegerValue(1000000));
		clientApps.Add(ulClient.Install(ueNodes.Get(u)));
	}
	serverApps.Start(MilliSeconds(500));
	clientApps.Start(MilliSeconds(500));
	lteHelper->EnableRlcTraces();
	lteHelper->EnableMacTraces();
	Simulator::Stop(simTime);
	Simulator::Run();
	Simulator::Destroy();
	return 0;
}
