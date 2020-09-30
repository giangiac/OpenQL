/**
 * @file   visualizer.cc
 * @date   08/2020
 * @author Tim van der Meer
 * @brief  definition of the visualizer
 */
 
#include "visualizer.h"
#include "visualizer_internal.h"
#include "json.h"
#include "instruction_map.h"

#include <iostream>
#include <limits>

namespace ql
{
// --- DONE ---
// visualization of custom gates
// option to enable or disable classical bit lines
// different types of cycle/duration(ns) labels
// gate duration outlines in gate color
// measurement without explicitly specified classical operand assumes default classical operand (same number as qubit number)
// read cycle duration from hardware config file, instead of having hardcoded value
// handle case where user does not or incorrectly specifies visualization nodes for custom gate
// allow the user to set the layout parameters from a configuration file
// implement a generic grid structure object to contain the visual structure of the circuit, to ease positioning of components in all the drawing functions
// change IOUT to DOUT (IOUT is used to avoid debug information from other source files while developing the visualizer!)
// visual_type attribute instead of full visual attribute in hw config file, links to seperate visualization config file where details of that visual type are detailed
// 'cutting' circuits where nothing/not much is happening both in terms of idle cycles and idle qubits

// -- IN PROGRESS ---
// add bit line zigzag indicating a cut cycle range
// add cutEmptyCycles and emptyCycleThreshold to the documentation

// --- FUTURE WORK ---
// TODO: the visualizer should probably be a class
// TODO: when gate is skipped due to whatever reason, maybe show a dummy gate outline indicating where the gate is?
// TODO: display wait/barrier gate (need wait gate fix first)
// TODO: fix overlapping connections for multiqubit gates/measurements
// TODO: representing the gates as waveforms (see andreas paper for examples)
// TODO: add classical bit number to measurement connection when classical lines are grouped
// TODO: implement measurement symbol (to replace the M on measurement gates)
// TODO: generate default gate visuals from the configuration file
// TODO: add option to save the image and/or open the window

#ifndef WITH_VISUALIZER

void visualize(const ql::quantum_program* program, const std::string& configPath)
{
	WOUT("The visualizer is disabled. If this was not intended, the X11 library might be missing and the visualizer has disabled itself.");
}

#else

using json = nlohmann::json;

Structure::Structure(const Layout layout, const CircuitData circuitData, const std::vector<EndPoints> cutCycleRanges)
	: layout(layout), circuitData(circuitData), cutCycleRanges(cutCycleRanges)
{
    DOUT("Calculating image properties...");
	
	labelColumnWidth = layout.bitLines.drawLabels ? layout.bitLines.labelColumnWidth : 0;
	cycleNumbersRowHeight = layout.cycles.showCycleLabels ? layout.cycles.rowHeight : 0;

	// Calculate the amount of displayed cycles.
	int amountOfCutCycles = 0;
	for (const auto& range : cutCycleRanges)
	{
		amountOfCutCycles += range.end - range.start + 1;
		// Add each cycle in the range to the vector with cut cycles for later use.
		for (int i = range.start; i <= range.end; i++)
		{
			cutCycles.push_back(i);
		}
	}

	int widthFromCycles = 0;
	if (layout.cycles.cutEmptyCycles)
	{
		widthFromCycles = cutCycleRanges.size() * layout.cycles.cutCycleWidth + (circuitData.amountOfCycles - amountOfCutCycles) * layout.grid.cellSize;
	}
	else
	{
		widthFromCycles = circuitData.amountOfCycles * layout.grid.cellSize;
	}
	imageWidth = layout.grid.borderSize * 2 + labelColumnWidth + widthFromCycles;

	// Calculate image height based on amount of quantum and classical bits.
	const int amountOfRows = circuitData.amountOfQubits + (layout.bitLines.groupClassicalLines ? (circuitData.amountOfClassicalBits > 0 ? 1 : 0) : circuitData.amountOfClassicalBits);
	imageHeight = (layout.cycles.showCycleLabels ? layout.cycles.rowHeight : 0) + amountOfRows * layout.grid.cellSize + 2 * layout.grid.borderSize;

	// Calculate the bit line segments.
	DOUT("Calculating bit line segments...");
	for (int i = 0; i < circuitData.amountOfCycles; i++)
	{
		bool cut = isCycleCut(i);
		bool reachedEnd = false;

		// Add more cycles to the segment until we reach a cycle that is cut if the current segment is not cut, or vice versa.
		for (int j = i; j < circuitData.amountOfCycles; j++)
		{
			if (isCycleCut(j) != cut)
			{
				const int start = getCellX(i);
				const int end = getCellX(j);
				DOUT("segment > range: [" << i << "," << (j - 1) << "], " << "position: [" << start << "," << end << "], cut: " << cut);
				bitLineSegments.push_back({{start, end}, cut});
				i = j - 1;
				break;
			}

			// Check if the last cycle has been reached, and exit the calculation if so.
			if (j == circuitData.amountOfCycles - 1)
			{
				const int start = getCellX(i);
				const int end = getCellX(j + 1);
				DOUT("segment > range: [" << i << "," << j << "], " << "position: [" << start << "," << end << "], cut: " << cut);
				bitLineSegments.push_back({{start, end}, cut});
				reachedEnd = true;
			}
		}

		if (reachedEnd) break;
	}
}

int Structure::getImageWidth() const
{
	return imageWidth;
}

int Structure::getImageHeight() const
{
	return imageHeight;
}

bool Structure::isCycleCut(const int cycle) const
{
	for (const int cutCycle : cutCycles)
	{
		if (cycle == cutCycle)
		{
			return true;
		}
	}

	return false;
}

bool Structure::isCycleFirstInCutRange(const int cycle) const
{
	for (const EndPoints& range : cutCycleRanges)
	{
		if (cycle == range.start)
		{
			return true;
		}
	}

	return false;
}

int Structure::getCellX(const int col) const
{
	int widthFromCycles = 0;
	if (layout.cycles.cutEmptyCycles)
	{
		int cutCycleRangesBeforeColumn = 0;
		int cutCyclesBeforeColumn = 0;
		for (const auto& range : cutCycleRanges)
		{
			if (col > range.end)
			{
				cutCycleRangesBeforeColumn++;
				cutCyclesBeforeColumn += range.end - range.start + 1;
			}
		}
		widthFromCycles = (col - cutCyclesBeforeColumn) * layout.grid.cellSize + cutCycleRangesBeforeColumn * layout.cycles.cutCycleWidth;
	}
	else
	{
		widthFromCycles = col * layout.grid.cellSize;
	}

	return layout.grid.borderSize + labelColumnWidth + widthFromCycles;
}

int Structure::getCellY(const int row) const
{
	return layout.grid.borderSize + cycleNumbersRowHeight + row * layout.grid.cellSize;
}

int Structure::getCycleLabelsY() const
{
	return layout.grid.borderSize;
}

int Structure::getBitLabelsX() const
{
	return layout.grid.borderSize;
}

std::vector<std::pair<EndPoints, bool>> Structure::getBitLineSegments() const
{
	return bitLineSegments;
}

void Structure::printProperties() const
{
	DOUT("-------------------------------------------------------------------------------");
	DOUT("IMAGE PROPERTIES:");
	DOUT("image width: " << imageWidth);
	DOUT("image height: " << imageHeight);
	DOUT("-------------------------------------------------------------------------------");
	DOUT("CUT CYCLES:");
	for (const int cycle : cutCycles)
	{
		DOUT(cycle);
	}
	DOUT("-------------------------------------------------------------------------------");
	DOUT("CYCLE POSITIONS:");
	for (int i = 0; i < circuitData.amountOfCycles; i++)
	{
		DOUT("cycle: " << i << ", x: " << getCellX(i));
	}
	DOUT("-------------------------------------------------------------------------------");
	// IOUT("BIT LINE POSITIONS:");
	// for (int i = 0; i < circuitData.amountOfQubits; i++)
	// {
	// 	IOUT("qbit line: " << i << ", y: " << getCellY(i));
	// }
	// for (int i = circuitData.amountOfQubits; i < circuitData.amountOfQubits + circuitData.amountOfClassicalBits; i++)
	// {
	// 	IOUT("cbit line: " << (i - circuitData.amountOfQubits) << ", y: " << getCellY(i));
	// }
	// IOUT("-------------------------------------------------------------------------------");
}

void visualize(const ql::quantum_program* program, const std::string& configPath)
{
    IOUT("Starting visualization...");

	DOUT("Parsing visualizer configuration file.");
	Layout layout = parseConfiguration(configPath);
	
    DOUT("Validating layout...");
	validateLayout(layout);

    // Get the gate list from the program.
    DOUT("Getting gate list...");
    std::vector<ql::gate*> gates;
    std::vector<ql::quantum_kernel> kernels = program->kernels;
    for (ql::quantum_kernel kernel : kernels)
    {
        circuit c = kernel.get_circuit();
        gates.insert( gates.end(), c.begin(), c.end() );
    }
    
	// Calculate amount of cycles and read cycle duration.
    DOUT("Calculating amount of cycles...");
	const int cycleDuration = program->platform.cycle_time;
	DOUT("Cycle duration is: " + std::to_string(cycleDuration) + " ns.");
    int amountOfCycles = calculateAmountOfCycles(gates, cycleDuration);

	// Compress the circuit in terms of cycles and gate duration if the option has been set.
	if (layout.cycles.compressCycles)
	{
		compressCycles(gates, amountOfCycles);
	}

	// Cut empty cycles if wanted.
	std::vector<EndPoints> cutCycleRanges;
	if (layout.cycles.cutEmptyCycles)
	{
		cutCycleRanges = findCuttableEmptyRanges(gates, layout, amountOfCycles);
	}

	// Calculate amount of qubits and classical bits.
    DOUT("Calculating amount of qubits and classical bits...");
	fixMeasurementOperands(gates); // fixes measurement gates without classical operands
	const int amountOfQubits = calculateAmountOfBits(gates, &gate::operands);
	const int amountOfClassicalBits = calculateAmountOfBits(gates, &gate::creg_operands);
	CircuitData circuitData = { amountOfQubits, amountOfClassicalBits, amountOfCycles, cycleDuration };
    
	// Initialize the structure of the visualization.
	DOUT("Initializing visualization structure...");
	Structure structure(layout, circuitData, cutCycleRanges);
	structure.printProperties();
	
	// Initialize image.
    DOUT("Initializing image...");
	const int numberOfChannels = 3;
	cimg_library::CImg<unsigned char> image(structure.getImageWidth(), structure.getImageHeight(), 1, numberOfChannels);
	image.fill(255);

	// Draw the cycle labels if the option has been set.
	if (layout.cycles.showCycleLabels)
	{
        DOUT("Drawing cycle numbers...");
		drawCycleLabels(image, layout, circuitData, structure);
	}

	// Draw the quantum bit lines.
    DOUT("Drawing qubit lines...");
	for (int i = 0; i < amountOfQubits; i++)
	{
		drawBitLine(image, layout, QUANTUM, i, circuitData, structure);
	}
	
	// Draw the classical lines if enabled.
	if (layout.bitLines.showClassicalLines)
	{
		// Draw the grouped classical bit lines if the option is set.
		if (amountOfClassicalBits > 0 && layout.bitLines.groupClassicalLines)
		{
			DOUT("Drawing grouped classical bit lines...");
			drawGroupedClassicalBitLine(image, layout, circuitData, structure);
		}
		// Otherwise draw each classical bit line seperate.
		else
		{
			DOUT("Drawing ungrouped classical bit lines...");
			for (int i = amountOfQubits; i < amountOfQubits + amountOfClassicalBits; i++)
			{
				drawBitLine(image, layout, CLASSICAL, i, circuitData, structure);
			}
		}
	}

	// Draw the gates.
    DOUT("Drawing gates...");
	for (gate* gate : gates)
	{
        //const GateVisual gateVisual = layout.gateVisuals.at(gate->type());
        DOUT("Drawing gate: [name: " + gate->name + "]");
		drawGate(image, layout, circuitData, gate, structure);
	}
	
	// Display the image.
    DOUT("Displaying image...");
	image.display("Quantum Circuit");

    IOUT("Visualization complete...");
}

Layout parseConfiguration(const std::string& configPath)
{
	json config;
	try
	{
		config = load_json(configPath);
	}
	catch (json::exception &e)
	{
		FATAL("Failed to load the visualization config file: malformed json file: \n\t" << std::string(e.what()));
	}

	Layout layout;

	// Fill the layout object with the values from the config file, or if those values are missing, with the default hardcoded values.
	//TODO: replace these hardcoded assignments by automatic json to layout object mapping (should be possible with nlohmann json)
	if (config.count("cycles") == 1)
	{
		layout.cycles.showCycleLabels = config["cycles"].value("showCycleLabels", layout.cycles.showCycleLabels);
		layout.cycles.showCyclesInNanoSeconds = config["cycles"].value("showCyclesInNanoSeconds", layout.cycles.showCyclesInNanoSeconds);
		layout.cycles.rowHeight = config["cycles"].value("rowHeight", layout.cycles.rowHeight);
		layout.cycles.fontHeight = config["cycles"].value("fontHeight", layout.cycles.fontHeight);
		layout.cycles.fontColor = config["cycles"].value("fontColor", layout.cycles.fontColor);

		layout.cycles.compressCycles = config["cycles"].value("compressCycles", layout.cycles.compressCycles);
		layout.cycles.cutEmptyCycles = config["cycles"].value("cutEmptyCycles", layout.cycles.cutEmptyCycles);
		layout.cycles.emptyCycleThreshold = config["cycles"].value("emptyCycleThreshold", layout.cycles.emptyCycleThreshold);
		layout.cycles.cutCycleWidth = config["cycles"].value("cutCycleWidth", layout.cycles.cutCycleWidth);
		layout.cycles.showGateDurationOutline = config["cycles"].value("showGateDurationOutline", layout.cycles.showGateDurationOutline);
		layout.cycles.gateDurationGap = config["cycles"].value("gateDurationGap", layout.cycles.gateDurationGap);
		layout.cycles.gateDurationAlpha = config["cycles"].value("gateDurationAlpha", layout.cycles.gateDurationAlpha);
		layout.cycles.gateDurationOutLineAlpha = config["cycles"].value("gateDurationOutLineAlpha", layout.cycles.gateDurationOutLineAlpha);
		layout.cycles.gateDurationOutlineColor = config["cycles"].value("gateDurationOutlineColor", layout.cycles.gateDurationOutlineColor);
	}

	if (config.count("bitLines") == 1)
	{
		layout.bitLines.drawLabels = config["bitLines"].value("drawLabels", layout.bitLines.drawLabels);
		layout.bitLines.labelColumnWidth = config["bitLines"].value("labelColumnWidth", layout.bitLines.labelColumnWidth);
		layout.bitLines.fontHeight = config["bitLines"].value("fontHeight", layout.bitLines.fontHeight);
		layout.bitLines.qBitLabelColor = config["bitLines"].value("qBitLabelColor", layout.bitLines.qBitLabelColor);
		layout.bitLines.cBitLabelColor = config["bitLines"].value("cBitLabelColor", layout.bitLines.cBitLabelColor);

		layout.bitLines.showClassicalLines = config["bitLines"].value("showClassicalLines", layout.bitLines.showClassicalLines);
		layout.bitLines.groupClassicalLines = config["bitLines"].value("groupClassicalLines", layout.bitLines.groupClassicalLines);
		layout.bitLines.groupedClassicalLineGap = config["bitLines"].value("groupedClassicalLineGap", layout.bitLines.groupedClassicalLineGap);
		layout.bitLines.qBitLineColor = config["bitLines"].value("qBitLineColor", layout.bitLines.qBitLineColor);
		layout.bitLines.cBitLineColor = config["bitLines"].value("cBitLineColor", layout.bitLines.cBitLineColor);
	}

	if (config.count("grid") == 1)
	{
		layout.grid.cellSize = config["grid"].value("cellSize", layout.grid.cellSize);
		layout.grid.borderSize = config["grid"].value("borderSize", layout.grid.borderSize);
	}
	
	if (config.count("measurements") == 1)
	{
		layout.measurements.drawConnection = config["measurements"].value("drawConnection", layout.measurements.drawConnection);
		layout.measurements.lineSpacing = config["measurements"].value("lineSpacing", layout.measurements.lineSpacing);
		layout.measurements.arrowSize = config["measurements"].value("arrowSize", layout.measurements.arrowSize);
	}

	// Load the custom instruction visualization parameters.
	if (config.count("instructions") == 1)
	{
		for (const auto& instruction : config["instructions"].items())
		{
			try
			{
				GateVisual gateVisual;
				json content = instruction.value();

				// Load the connection color.
				json connectionColor = content["connectionColor"];
				gateVisual.connectionColor[0] = connectionColor[0];
				gateVisual.connectionColor[1] = connectionColor[1];
				gateVisual.connectionColor[2] = connectionColor[2];
				DOUT("Connection color: [" 
					<< (int)gateVisual.connectionColor[0] << ","
					<< (int)gateVisual.connectionColor[1] << ","
					<< (int)gateVisual.connectionColor[2] << "]");

				// Load the individual nodes.
				json nodes = content["nodes"];
				int amountOfNodes = nodes.size();
				for (int i = 0; i < amountOfNodes; i++)
				{
					json node = nodes[i];
					
					std::array<unsigned char, 3> fontColor = {node["fontColor"][0], node["fontColor"][1], node["fontColor"][2]};
					std::array<unsigned char, 3> backgroundColor = {node["backgroundColor"][0], node["backgroundColor"][1], node["backgroundColor"][2]};
					std::array<unsigned char, 3> outlineColor = {node["outlineColor"][0], node["outlineColor"][1], node["outlineColor"][2]};
					
					NodeType nodeType;
					if (node["type"] == "NONE") {nodeType = NONE;} else
					if (node["type"] == "GATE") {nodeType = GATE;} else
					if (node["type"] == "CONTROL") {nodeType = CONTROL;} else
					if (node["type"] == "NOT") {nodeType = NOT;} else
					if (node["type"] == "CROSS") {nodeType = CROSS;}
					else
					{
						WOUT("Unknown gate display node type! Defaulting to type NONE...");
						nodeType = NONE;
					}
					
					Node loadedNode = 
					{
						nodeType,
						node["radius"],
						node["displayName"],
						node["fontHeight"],
						fontColor,
						backgroundColor,
						outlineColor
					};
					
					gateVisual.nodes.push_back(loadedNode);
					
					DOUT("[type: " << node["type"] << "] "
						<< "[radius: " << gateVisual.nodes.at(i).radius << "] "
						<< "[displayName: " << gateVisual.nodes.at(i).displayName << "] "
						<< "[fontHeight: " << gateVisual.nodes.at(i).fontHeight << "] "
						<< "[fontColor: "
							<< (int)gateVisual.nodes.at(i).fontColor[0] << ","
							<< (int)gateVisual.nodes.at(i).fontColor[1] << ","
							<< (int)gateVisual.nodes.at(i).fontColor[2] << "] "
						<< "[backgroundColor: "
							<< (int)gateVisual.nodes.at(i).backgroundColor[0] << ","
							<< (int)gateVisual.nodes.at(i).backgroundColor[1] << ","
							<< (int)gateVisual.nodes.at(i).backgroundColor[2] << "] "
						<< "[outlineColor: "
							<< (int)gateVisual.nodes.at(i).outlineColor[0] << ","
							<< (int)gateVisual.nodes.at(i).outlineColor[1] << ","
							<< (int)gateVisual.nodes.at(i).outlineColor[2] << "]");
				}

				layout.customGateVisuals.insert({instruction.key(), gateVisual});
			}
			catch (json::exception &e)
			{
				WOUT("Failed to load visualization parameters for instruction: '" << instruction.key() << "' \n\t" << std::string(e.what()));
			}
		}
	}
	else
	{
		WOUT("Did not find 'instructions' attribute! The visualizer will try to fall back on default gate visualizations.");
	}

	return layout;
}

void validateLayout(Layout& layout)
{
	//TODO: add more validation
	
	if (layout.cycles.emptyCycleThreshold < 1)
	{
		WOUT("Adjusting 'emptyCycleThreshold' to minimum value of 1. Value in configuration file is set to " << layout.cycles.emptyCycleThreshold << ".");
		layout.cycles.emptyCycleThreshold = 1;
	}
}

int calculateAmountOfBits(const std::vector<ql::gate*> gates, const std::vector<size_t> ql::gate::* operandType)
{
	//TODO: handle circuits not starting at the c- or q-bit with index 0
	int minAmount = std::numeric_limits<int>::max();
	int maxAmount = 0;

	for (const gate* gate : gates)
	{
		std::vector<size_t>::const_iterator begin = (gate->*operandType).begin();
		const std::vector<size_t>::const_iterator end = (gate->*operandType).end();
		
		for (; begin != end; ++begin)
		{
			const size_t number = *begin;
			if (number < minAmount)
				minAmount = (int) number;
			if (number > maxAmount)
				maxAmount = (int) number;
		}
	}

	// If both minAmount and maxAmount are at their original values, the list of operands for all the gates was empty.
	// This means there are no operands of the given type for these gates and we return 0.
	if (minAmount == std::numeric_limits<int>::max() && maxAmount == 0)
		return 0;
	else
		return 1 + maxAmount - minAmount; // +1 because: max - min = #qubits - 1
}

int calculateAmountOfCycles(const std::vector<ql::gate*> gates, const int cycleDuration)
{
    int amountOfCycles = 0;
	for (const gate* gate : gates)
	{
		const int gateCycle = (int)gate->cycle;
		if (gateCycle > amountOfCycles)
			amountOfCycles = gateCycle;
	}
	amountOfCycles++; // because the cycles start at zero, we add one to get the true amount of cycles
	const gate* lastGate = gates.at(gates.size() - 1);
	const int lastGateDuration = (int)lastGate->duration;
	const int lastGateDurationInCycles = lastGateDuration / cycleDuration;
	if (lastGateDurationInCycles > 1)
	{
		amountOfCycles += lastGateDurationInCycles - 1;
	}

    return amountOfCycles;
}

int calculateAmountOfGateOperands(const ql::gate* gate)
{
	return (int)gate->operands.size() + (int)gate->creg_operands.size();
}

void compressCycles(const std::vector<ql::gate*> gates, int& amountOfCycles)
{
	DOUT("Compressing circuit...");
	std::vector<bool> filledCycles(amountOfCycles);
	for (int i = 0; i < gates.size(); i++)
	{
		filledCycles.at(gates.at(i)->cycle) = true;
	}

	DOUT("amount of cycles before compression: " << amountOfCycles);
	int amountOfCompressions = 0;
	for (int i = 0; i < filledCycles.size(); i++)
	{
		DOUT(i);
		if (filledCycles.at(i) == false)
		{
			DOUT(" not filled");
			DOUT("\tcompressing... min cycle to compress: " << i - amountOfCompressions);
			for (int j = 0; j < gates.size(); j++)
			{
				const int gateCycle = (int)gates.at(j)->cycle;
				DOUT("\tgate cycle: " << gateCycle);
				if (gateCycle >= i - amountOfCompressions)
				{
					gates.at(j)->cycle = gates.at(j)->cycle - 1;
					DOUT(" -> compressing cycle");
				}
				else
				{
					DOUT(" -> no compression");
				}
			}
			amountOfCycles--;
			amountOfCompressions++;
		}
		else
		{
			DOUT(" filled");
		}
	}
	DOUT("amount of cycles after compression: " << amountOfCycles);
}

std::vector<EndPoints> findCuttableEmptyRanges(const std::vector<ql::gate*> gates, const Layout layout, const int amountOfCycles)
{
	DOUT("Checking for empty cycle ranges...");

	std::vector<bool> cycles(amountOfCycles, true);

	for (const gate* gate : gates)
	{
		cycles[gate->cycle] = false;
	}

	// Calculate the empty cycle ranges.
	std::vector<EndPoints> ranges;
	for (int i = 0; i < cycles.size(); i++)
	{
		// If an empty cycle has been found...
		if (cycles[i] == true)
		{
			const int start = i;
			int end = cycles.size() - 1;

			int j = i;
			// ... add cycles to the range until a non-empty cycle is found.
			while (j < cycles.size())
			{
				if (cycles[j] == false)
				{
					end = j - 1;
					break;
				}
				j++;
			}
			ranges.push_back({start, end});

			// Skip over the found range.
			i = j;
		}
	}

	// Check for empty cycle ranges above the threshold.
	std::vector<EndPoints> rangesAboveThreshold;
	for (const auto& range : ranges)
	{
		const int length = range.end - range.start + 1;
		DOUT("Range from " << range.start << " to " << range.end << " with length " << length << ".");

		if (length >= layout.cycles.emptyCycleThreshold)
		{
			DOUT("Found empty cycle range above threshold.");
			rangesAboveThreshold.push_back(range);
		}
	}

	return rangesAboveThreshold;
}

void fixMeasurementOperands(const std::vector<ql::gate*> gates)
{
	for (gate* gate : gates)
	{
		// Check for a measurement gate without explicitly specified classical operand.
		if (isMeasurement(gate))
		{
			if (calculateAmountOfGateOperands(gate) == 1)
			{
				// Set classical measurement operand to the bit corresponding to the measuremens qubit number.
				DOUT("Found measurement gate with no classical operand. Assuming default classical operand.");
				const int cbit = gate->operands[0];
				gate->creg_operands.push_back(cbit);
			}
		}
	}
}

bool isMeasurement(const ql::gate* gate)
{
	//TODO: this method of checking for measurement gates is not very robust and relies entirely on the user naming their instructions in a certain way!
	return (gate->name.find("measure") != std::string::npos);
}

Dimensions calculateTextDimensions(const std::string& text, const int fontHeight, const Layout layout)
{
	const char* chars = text.c_str();
	cimg_library::CImg<unsigned char> imageTextDimensions;
	const char color = 1;
	imageTextDimensions.draw_text(0, 0, chars, &color, 0, 1, fontHeight);

	return Dimensions { (int) imageTextDimensions.width(), (int) imageTextDimensions.height() };
}

void drawCycleLabels(cimg_library::CImg<unsigned char>& image, const Layout layout, const CircuitData circuitData, const Structure structure)
{
	for (int i = 0; i < circuitData.amountOfCycles; i++)
	{
		std::string cycleLabel = "";
		int cellWidth = 0;
		if (structure.isCycleCut(i))
		{
			if (!structure.isCycleFirstInCutRange(i))
			{
				continue;
			}
			cellWidth = layout.cycles.cutCycleWidth;
			cycleLabel = "...";
		}
		else
		{
			cellWidth = layout.grid.cellSize;
			if (layout.cycles.showCyclesInNanoSeconds)
			{
				cycleLabel = std::to_string(i * circuitData.cycleDuration);
			}
			else
			{
				cycleLabel = std::to_string(i);
			}
		}

		Dimensions textDimensions = calculateTextDimensions(cycleLabel, layout.cycles.fontHeight, layout);

		const int xGap = (cellWidth - textDimensions.width) / 2;
		const int yGap = (layout.cycles.rowHeight - textDimensions.height) / 2;
		const int xCycle = structure.getCellX(i) + xGap;
		const int yCycle = structure.getCycleLabelsY() + yGap;

		image.draw_text(xCycle, yCycle, cycleLabel.c_str(), layout.cycles.fontColor.data(), 0, 1, layout.cycles.fontHeight);
	}
}

void drawBitLine(cimg_library::CImg<unsigned char> &image, const Layout layout, const BitType bitType, const int row, const CircuitData circuitData, const Structure structure)
{
	std::array<unsigned char, 3> bitLineColor;
	std::array<unsigned char, 3> bitLabelColor;
	switch (bitType)
	{
		case CLASSICAL:
			bitLineColor = layout.bitLines.cBitLineColor;
			bitLabelColor = layout.bitLines.cBitLabelColor;
			break;
		case QUANTUM:
			bitLineColor = layout.bitLines.qBitLineColor;
			bitLabelColor = layout.bitLines.qBitLabelColor;
			break;
	}

	for (const std::pair<EndPoints, bool>& segment : structure.getBitLineSegments())
	{
		// Check if the segment is a cut segment.
		if (segment.second == true)
		{
			const int y = structure.getCellY(row) + layout.grid.cellSize / 2;
			const int height = layout.grid.cellSize / 8;
			const int width = segment.first.end - segment.first.start;
			
			image.draw_line(segment.first.start,					y,			segment.first.start + width / 3,		y - height,	bitLineColor.data());
			image.draw_line(segment.first.start + width / 3,		y - height,	segment.first.start + width / 3 * 2,	y + height,	bitLineColor.data());
			image.draw_line(segment.first.start + width / 3 * 2,	y + height,	segment.first.end,						y,			bitLineColor.data());
		}
		else
		{
			const int y = structure.getCellY(row) + layout.grid.cellSize / 2;
			image.draw_line(segment.first.start, y, segment.first.end, y, bitLineColor.data());
		}
	}

	// Draw the bit line label if enabled.
	if (layout.bitLines.drawLabels)
	{
		const int bitIndex = (bitType == CLASSICAL) ? (row - circuitData.amountOfQubits) : row;
		const std::string bitTypeText = (bitType == CLASSICAL) ? "c" : "q";
		std::string label = bitTypeText + std::to_string(bitIndex);
		Dimensions textDimensions = calculateTextDimensions(label, layout.bitLines.fontHeight, layout);

		const int xGap = (layout.grid.cellSize - textDimensions.width) / 2;
		const int yGap = (layout.grid.cellSize - textDimensions.height) / 2;
		const int xLabel = structure.getBitLabelsX() + xGap;
		const int yLabel = structure.getCellY(row) + yGap;

		image.draw_text(xLabel, yLabel, label.c_str(), bitLabelColor.data(), 0, 1, layout.bitLines.fontHeight);
	}
}

void drawGroupedClassicalBitLine(cimg_library::CImg<unsigned char>& image, const Layout layout, const CircuitData circuitData, const Structure structure)
{
	const int y = structure.getCellY(circuitData.amountOfQubits) + layout.grid.cellSize / 2;

	// Draw the segments of the double line.
	for (const std::pair<EndPoints, bool>& segment : structure.getBitLineSegments())
	{
		// Check if the segment is a cut segment.
		if (segment.second == true)
		{
			const int height = layout.grid.cellSize / 8;
			const int width = segment.first.end - segment.first.start;
			
			image.draw_line(segment.first.start, y - layout.bitLines.groupedClassicalLineGap,
							segment.first.start + width / 3, y - height - layout.bitLines.groupedClassicalLineGap,
							layout.bitLines.cBitLineColor.data());
			image.draw_line(segment.first.start + width / 3, y - height - layout.bitLines.groupedClassicalLineGap,
							segment.first.start + width / 3 * 2, y + height - layout.bitLines.groupedClassicalLineGap,
							layout.bitLines.cBitLineColor.data());
			image.draw_line(segment.first.start + width / 3 * 2, y + height - layout.bitLines.groupedClassicalLineGap,
							segment.first.end, y - layout.bitLines.groupedClassicalLineGap,
							layout.bitLines.cBitLineColor.data());

			image.draw_line(segment.first.start, y + layout.bitLines.groupedClassicalLineGap,
							segment.first.start + width / 3, y - height + layout.bitLines.groupedClassicalLineGap,
							layout.bitLines.cBitLineColor.data());
			image.draw_line(segment.first.start + width / 3, y - height + layout.bitLines.groupedClassicalLineGap,
							segment.first.start + width / 3 * 2, y + height + layout.bitLines.groupedClassicalLineGap,
							layout.bitLines.cBitLineColor.data());
			image.draw_line(segment.first.start + width / 3 * 2, y + height + layout.bitLines.groupedClassicalLineGap,
							segment.first.end, y + layout.bitLines.groupedClassicalLineGap,
							layout.bitLines.cBitLineColor.data());
		}
		else
		{
			image.draw_line(segment.first.start, y - layout.bitLines.groupedClassicalLineGap, 
				segment.first.end, y - layout.bitLines.groupedClassicalLineGap, layout.bitLines.cBitLineColor.data());
			image.draw_line(segment.first.start, y + layout.bitLines.groupedClassicalLineGap, 
				segment.first.end, y + layout.bitLines.groupedClassicalLineGap, layout.bitLines.cBitLineColor.data());	
		}
	}

	// Draw the dashed line plus classical bit amount number on the first segment.
	std::pair<EndPoints, bool> firstSegment = structure.getBitLineSegments()[0];
	//TODO: store the dashed line parameters in the layout object
	image.draw_line(firstSegment.first.start + 8, y + layout.bitLines.groupedClassicalLineGap + 2, firstSegment.first.start + 12, y - layout.bitLines.groupedClassicalLineGap - 3, layout.bitLines.cBitLineColor.data());
	const std::string label = std::to_string(circuitData.amountOfClassicalBits);
	//TODO: fix these hardcoded parameters
	const int xLabel = firstSegment.first.start + 8;
	const int yLabel = y - layout.bitLines.groupedClassicalLineGap - 3 - 13;
	image.draw_text(xLabel, yLabel, label.c_str(), layout.bitLines.cBitLabelColor.data(), 0, 1, layout.bitLines.fontHeight);

	// Draw the bit line label if enabled.
	if (layout.bitLines.drawLabels)
	{
		const std::string label = "C";
		Dimensions textDimensions = calculateTextDimensions(label, layout.bitLines.fontHeight, layout);

		const int xGap = (layout.grid.cellSize - textDimensions.width) / 2;
		const int yGap = (layout.grid.cellSize - textDimensions.height) / 2;
		const int xLabel = structure.getBitLabelsX() + xGap;
		const int yLabel = structure.getCellY(circuitData.amountOfQubits) + yGap;

		image.draw_text(xLabel, yLabel, label.c_str(), layout.bitLines.cBitLabelColor.data(), 0, 1, layout.bitLines.fontHeight);
	}
}

void drawGate(cimg_library::CImg<unsigned char> &image, const Layout layout, const CircuitData circuitData, gate* const gate, const Structure structure)
{
	DOUT("Drawing gate with name: '" << gate->name << "'");
	
	GateVisual gateVisual;
	if (gate->type() == __custom_gate__)
	{
		if (layout.customGateVisuals.count(gate->visual_type) == 1)
		{
			DOUT("Found visual for custom gate: '" << gate->name << "'");
			gateVisual = layout.customGateVisuals.at(gate->visual_type);
		}
		else
		{
			// TODO: try to recover by matching gate name with a default visual name
			// TODO: if the above fails, display a dummy gate
			WOUT("Did not find visual for custom gate: '" << gate->name << "', skipping gate!");
			return;
		}
	}
	else
	{
		DOUT("Default gate found. Using default visualization!");
		gateVisual = layout.defaultGateVisuals.at(gate->type());
	}

	const int amountOfOperands = calculateAmountOfGateOperands(gate);

	// Check for correct amount of nodes.
	if (amountOfOperands != gateVisual.nodes.size())
	{
		WOUT("Amount of gate operands and visualization nodes are not equal. Skipping gate with name: '" << gate->name << "' ...");
		return;
	}

	if (amountOfOperands > 1)
	{
        DOUT("Setting up multi-operand gate...");
		// Draw the lines between each node. If this is done before drawing the nodes, there is no need to calculate line segments, we can just draw one
		// big line between the nodes and the nodes will be drawn on top of those.
		// Note: does not work with transparent nodes! If those are ever implemented, the connection line drawing will need to be changed!

		int minRow = std::numeric_limits<int>::max();
		int maxRow = 0;
		for (int i = 0; i < gate->operands.size(); i++)
		{
			const int operand = gate->operands.at(i);
			if (operand < minRow)
				minRow = operand;
			if (operand > maxRow)
				maxRow = operand;
		}
		if (layout.bitLines.groupClassicalLines)
		{
			if (gate->creg_operands.size() > 0)
			{
				maxRow = circuitData.amountOfQubits;
			}
		}
		else
		{
			for (int i = 0; i < gate->creg_operands.size(); i++)
			{
				const int operand = gate->creg_operands.at(i) + circuitData.amountOfQubits;
				if (operand < minRow)
					minRow = operand;
				if (operand > maxRow)
					maxRow = operand;
			}
		}
		const int column = (int)gate->cycle;

		Position4 connectionPosition =
		{
			structure.getCellX(column) + layout.grid.cellSize / 2,
			structure.getCellY(minRow) + layout.grid.cellSize / 2,
			structure.getCellX(column) + layout.grid.cellSize / 2,
			structure.getCellY(maxRow) + layout.grid.cellSize / 2,
		};

		//TODO: probably have connection line type as part of a gate's visual definition
		if (isMeasurement(gate))
		{
			if (layout.measurements.drawConnection && layout.bitLines.showClassicalLines)
			{
				const int groupedClassicalLineOffset = layout.bitLines.groupClassicalLines ? layout.bitLines.groupedClassicalLineGap : 0;

				image.draw_line(connectionPosition.x0 - layout.measurements.lineSpacing, connectionPosition.y0,
					connectionPosition.x1 - layout.measurements.lineSpacing, connectionPosition.y1 - layout.measurements.arrowSize - groupedClassicalLineOffset,
					gateVisual.connectionColor.data());

				image.draw_line(connectionPosition.x0 + layout.measurements.lineSpacing, connectionPosition.y0,
					connectionPosition.x1 + layout.measurements.lineSpacing, connectionPosition.y1 - layout.measurements.arrowSize - groupedClassicalLineOffset,
					gateVisual.connectionColor.data());

				const int x0 = connectionPosition.x1 - layout.measurements.arrowSize / 2;
				const int y0 = connectionPosition.y1 - layout.measurements.arrowSize - groupedClassicalLineOffset;
				const int x1 = connectionPosition.x1 + layout.measurements.arrowSize / 2;
				const int y1 = connectionPosition.y1 - layout.measurements.arrowSize - groupedClassicalLineOffset;
				const int x2 = connectionPosition.x1;
				const int y2 = connectionPosition.y1 - groupedClassicalLineOffset;
				image.draw_triangle(x0, y0, x1, y1, x2, y2, gateVisual.connectionColor.data(), 1);
			}
		}
		else
		{
			image.draw_line(connectionPosition.x0, connectionPosition.y0, connectionPosition.x1, connectionPosition.y1, gateVisual.connectionColor.data());
		}
        DOUT("Finished setting up multi-operand gate");
	}

	// Draw the gate duration outline if the option has been set.
	if (!layout.cycles.compressCycles && layout.cycles.showGateDurationOutline)
	{
        DOUT("Drawing gate duration outline...");
		const int gateDurationInCycles = ((int)gate->duration) / circuitData.cycleDuration;
		// Only draw the gate outline if the gate takes more than one cycle.
		if (gateDurationInCycles > 1)
		{
			for (int i = 0; i < amountOfOperands; i++)
			{
				const int columnStart = (int)gate->cycle;
				const int columnEnd = columnStart + gateDurationInCycles - 1;
				const int row = gate->operands[i];

				const int x0 = structure.getCellX(columnStart) + layout.cycles.gateDurationGap;
				const int y0 = structure.getCellY(row) + layout.cycles.gateDurationGap;
				const int x1 = structure.getCellX(columnEnd + 1) - layout.cycles.gateDurationGap;
				const int y1 = structure.getCellY(row + 1) - layout.cycles.gateDurationGap;

				// Draw the outline in the colors of the node.
				const Node node = gateVisual.nodes.at(i);
				image.draw_rectangle(x0, y0, x1, y1, node.backgroundColor.data(), layout.cycles.gateDurationAlpha);
				image.draw_rectangle(x0, y0, x1, y1, node.outlineColor.data(), layout.cycles.gateDurationOutLineAlpha, 0xF0F0F0F0);
				
				//image.draw_rectangle(x0, y0, x1, y1, layout.cycles.gateDurationOutlineColor.data(), layout.cycles.gateDurationAlpha);
				//image.draw_rectangle(x0, y0, x1, y1, layout.cycles.gateDurationOutlineColor.data(), layout.cycles.gateDurationOutLineAlpha, 0xF0F0F0F0);
			}
		}
	}

	// Draw the nodes.
    DOUT("Drawing gate nodes...");
	for (int i = 0; i < amountOfOperands; i++)
	{
        DOUT("Drawing gate node with index: " + std::to_string(i) + "...");
        //TODO: change the try-catch later on! the gate config will be read from somewhere else than the default layout
        try
        {
		    const Node node = gateVisual.nodes.at(i);
            const BitType operandType = (i >= gate->operands.size()) ? CLASSICAL : QUANTUM;
            const int index = (operandType == QUANTUM) ? i : (i - (int)gate->operands.size());

			const Cell cell =
			{
				(int)gate->cycle,
	            operandType == CLASSICAL ? (int)gate->creg_operands.at(index) + circuitData.amountOfQubits : (int)gate->operands.at(index)
			};

            switch (node.type)
            {
	            case NONE:		DOUT("node.type = NONE"); break; // Do nothing.
	            case GATE:		DOUT("node.type = GATE"); drawGateNode(image, layout, structure, node, cell); break;
	            case CONTROL:	DOUT("node.type = CONTROL"); drawControlNode(image, layout, structure, node, cell); break;
	            case NOT:		DOUT("node.type = NOT"); drawNotNode(image, layout, structure, node, cell); break;
	            case CROSS:		DOUT("node.type = CROSS"); drawCrossNode(image, layout, structure, node, cell); break;
                default:        EOUT("Unknown gate display node type!"); break;
            }
        }
        catch (const std::out_of_range& e)
        {
			WOUT(std::string(e.what()));
            return;
        }
		
        DOUT("Finished drawing gate node with index: " + std::to_string(i) + "...");
	}

	// Draw the measurement symbol.
	// const unsigned int xGap = 2;
	// const unsigned int yGap = 13;

	// const unsigned int x0 = position.x0 + xGap;
	// const unsigned int y0 = position.y0 + yGap;
	// const unsigned int x1 = position.x1 + xGap;
	// const unsigned int y1 = y0;

	// const unsigned int xa = x0 + (x1 - x0) / 3;
	// const unsigned int ya = y0 + yGap / 2;
	// const unsigned int xb = x1 - (x1 - x0) / 3;
	// const unsigned int yb = ya;

	// const unsigned int u0 = xa - x0;
	// const unsigned int v0 = ya - y0;
	// const unsigned int u1 = x1 - xb;
	// const unsigned int v1 = y1 - yb;

	// image.draw_spline(x0, y0, u0, v0, x1, y1, u1, v1, layout.operation.gateNameColor.data());
}

void drawGateNode(cimg_library::CImg<unsigned char>& image, const Layout layout, const Structure structure, const Node node, const Cell cell)
{
	const int gap = (layout.grid.cellSize - node.radius * 2) / 2;
	const Position4 position =
	{
		structure.getCellX(cell.col) + gap,
		structure.getCellY(cell.row) + gap,
		structure.getCellX(cell.col + 1) - gap,
		structure.getCellY(cell.row + 1) - gap
	};

	// Draw the gate background.
	image.draw_rectangle(position.x0, position.y0, position.x1, position.y1, node.backgroundColor.data());
	image.draw_rectangle(position.x0, position.y0, position.x1, position.y1, node.outlineColor.data(), 1, 0xFFFFFFFF);

	// Draw the gate symbol. The width and height of the symbol are calculated first to correctly position the symbol within the gate.
	Dimensions textDimensions = calculateTextDimensions(node.displayName, node.fontHeight, layout);
	image.draw_text(position.x0 + (node.radius * 2 - textDimensions.width) / 2, position.y0 + (node.radius * 2 - textDimensions.height) / 2, node.displayName.c_str(), node.fontColor.data(), 0, 1, node.fontHeight);
}

void drawControlNode(cimg_library::CImg<unsigned char>& image, const Layout layout, const Structure structure, const Node node, const Cell cell)
{
	const Position2 position =
	{
		structure.getCellX(cell.col) + layout.grid.cellSize / 2,
		structure.getCellY(cell.row) + layout.grid.cellSize / 2
	};

	image.draw_circle(position.x, position.y, node.radius, node.backgroundColor.data());
}

void drawNotNode(cimg_library::CImg<unsigned char>& image, const Layout layout, const Structure structure, const Node node, const Cell cell)
{
	// TODO: allow for filled not node instead of only an outline not node

	const Position2 position =
	{
		structure.getCellX(cell.col) + layout.grid.cellSize / 2,
		structure.getCellY(cell.row) + layout.grid.cellSize / 2
	};

	// Draw the outlined circle.
	image.draw_circle(position.x, position.y, node.radius, node.backgroundColor.data(), 1, 0xFFFFFFFF);

	// Draw two lines to represent the plus sign.
	const int xHor0 = position.x - node.radius;
	const int xHor1 = position.x + node.radius;
	const int yHor = position.y;

	const int xVer = position.x;
	const int yVer0 = position.y - node.radius;
	const int yVer1 = position.y + node.radius;

	image.draw_line(xHor0, yHor, xHor1, yHor, node.backgroundColor.data());
	image.draw_line(xVer, yVer0, xVer, yVer1, node.backgroundColor.data());
}

void drawCrossNode(cimg_library::CImg<unsigned char>& image, const Layout layout, const Structure structure, const Node node, const Cell cell)
{
	const Position2 position =
	{
		structure.getCellX(cell.col) + layout.grid.cellSize / 2,
		structure.getCellY(cell.row) + layout.grid.cellSize / 2
	};

	// Draw two diagonal lines to represent the cross.
	const int x0 = position.x - node.radius;
	const int y0 = position.y - node.radius;
	const int x1 = position.x + node.radius;
	const int y1 = position.y + node.radius;

	image.draw_line(x0, y0, x1, y1, node.backgroundColor.data());
	image.draw_line(x0, y1, x1, y0, node.backgroundColor.data());
}

#endif //WITH_VISUALIZER

} // ql
