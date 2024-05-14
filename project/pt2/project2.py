# Authors: Karthik Suresh (suresk) and Serge Piskun (piskus)
# Project 2: CSCI 4210

import sys

processes = {}
memory = []
initialStartTime = {}
initialEndTime = {}
startTimes = {}
endTimes = {}
framePerLine = 0
numFrames = 0
currentPos = 0
defragTimePerMove = 0
currentTime = 0

def printMemory():
	global memory, numFrames, framePerLine
	count = 1
	tempstr = ""
	print ("="*framePerLine)
	for x in memory:
		tempstr += x
		if count == framePerLine:
			print ("{}".format(tempstr))
			count = 0
			tempstr = ""
		count += 1
	if tempstr != "":
		print ("{}".format(tempstr))
	print ("="*framePerLine)


def defrag(frameSize, time, pid, startTimeCounter, endTimeCounter):
	global currentPos, defragTimePerMove, currentTime, startTimes, endTimes, memory

	movedPids = []
	count = 0
	for x in memory:
		if x == '.':
			count += 1
	if count < frameSize:
		return -1
	
	movedProcesses = 0
	first = 0
	second = 0
	while first < len(memory)-1 and second < len(memory):
		if memory[first] == '.':
			while second < len(memory)-1 and memory[second] == '.':
				second += 1
		while second == len(memory) - 1 and first < len(memory)-1:
			memory[first] = '.'
			first += 1
		if first < len(memory) and second < len(memory):
			if memory[first] != memory[second] and memory[second] != '.':
				if memory[second] not in movedPids:
					movedPids.append(memory[second])
			memory[first] = memory[second]
			if memory[first] == '.':
				first -= 1
			first += 1
			second += 1
	currentPos = 0

	print ("time {}ms: Cannot place process {} -- starting defragmentation".format(time, pid))

	for x in movedPids:
		movedProcesses += processes[x]

	addedDefragTime = movedProcesses*int(defragTimePerMove)
	newTime = time + addedDefragTime

	startTimes = {(addedDefragTime)+(key) if (key >= time) else key: value
		for key, value in startTimes.items()}
	endTimes = {(addedDefragTime)+(key) if (key >= time) else key: value
		for key, value in endTimes.items()}

	movedPidsString = ""
	for x in movedPids:
		movedPidsString += x
		movedPidsString += ", "
	movedPidsString = movedPidsString[:-2]
	print ("time {}ms: Defragmentation complete (moved {} frames: {})".format(newTime, movedProcesses, movedPidsString))
	return newTime

def possibleToAddPid(frameSize):
	count = 0
	for x in memory:
		if x == '.':
			count += 1
	if count >= frameSize:
		return 0
	return -1

def findBestFitLocation(frameSize):
	global memory
	partitions = {}
	min_length = sys.maxsize
	start_pos = -1

	for x in range(0, len(memory)):
		y = x
		while y < len(memory) and memory[y] == '.':
			y += 1
		if y not in partitions and y != x:
			partitions[y] = x

	for key, value in partitions.items():
		currLen = key - value + 1
		if currLen > frameSize and currLen < min_length:
			min_length = currLen
			start_pos = value
	return start_pos

def findFirstFitLocation(frameSize):
	global memory

	for x in range(0, len(memory)):
		rVal = 1
		for y in range(x, x+frameSize):
			if y >= len(memory):
				return -1
			if memory[y] != '.':
				rVal = 0
				break
		if rVal == 1:
			return x
	return -1

def findNextFitLocation(frameSize):
	global currentPos 
	startVal = currentPos
	for x in range(startVal, len(memory)):
		rVal = 1
		for y in range(x, x+frameSize):
			if y >= len(memory) or memory[y] != '.':
				rVal = 0
				continue
		if rVal == 1:
			currentPos = x+frameSize
			return x
	for x in range(0, startVal):
		rVal = 1
		for y in range(x, x+frameSize):
			if y >= len(memory) or memory[y] != '.':
				rVal = 0
				continue
		if rVal == 1:
			currentPos = x+frameSize
			return x
	return -1



def runSim(algorithm):
	global currentTime, startTimes, endTimes, memory
	# currentTime = 0
	startTimeCounter = 0
	endTimeCounter = 0

	while(startTimeCounter < len(startTimes) or endTimeCounter < len(endTimes)):
		tempVal = 0
		curStartTime = sys.maxsize
		curEndTime = sys.maxsize

		for key in sorted(startTimes):
			if tempVal == startTimeCounter:
				curStartTime = key
				break
			tempVal +=1

		tempVal = 0
		for key in sorted(endTimes):
			if tempVal == endTimeCounter:
				curEndTime = key
				break
			tempVal +=1

		if endTimeCounter < len(endTimes) and curEndTime <= curStartTime:
			val = 0
			for key, value in sorted(endTimes.items()):
				if val == endTimeCounter:
					tempValues = value
					for pid in tempValues:
						ableToRemove = 0
						for x in range(0, len(memory)):
							if memory[x] == pid:
								memory[x] = '.'
								ableToRemove = 1
						if ableToRemove == 1:
							print ("time {}ms: Process {} removed:".format(key, pid))
							currentTime = key
							# print memory
							printMemory()
					break
				val+=1
			endTimeCounter+=1

		if startTimeCounter < len(startTimes) and curStartTime <= curEndTime:
			val = 0
			for key, value in sorted(startTimes.items()):
				if val == startTimeCounter:
					tempValues = value
					for pid in tempValues:
						frameSize = processes[pid]
						print ("time {}ms: Process {} arrived (requires {} frames)".format(key, pid, processes[pid]))
						currentTime = key

						if algorithm == "nonContig":
							startPos = possibleToAddPid(frameSize)
							if startPos != -1:
								print ("time {}ms: Placed process {}:".format(key, pid))
								count = 0
								for x in range(0, len(memory)):
									if memory[x] == '.':
										memory[x] = pid
										count += 1
									if count >= frameSize:
										break
								printMemory()

						elif algorithm == "first":
							startPos = findFirstFitLocation(frameSize)
							if startPos == -1:
								newKey = defrag(frameSize, key, pid, startTimeCounter, endTimeCounter)
								startPos = findFirstFitLocation(frameSize)
								if startPos != -1:
									key = newKey
						elif algorithm == "next":
							startPos = findNextFitLocation(frameSize)
							if startPos == -1:
								newKey = defrag(frameSize, key, pid, startTimeCounter, endTimeCounter)
								startPos = findNextFitLocation(frameSize)
								if startPos != -1:
									key = newKey
						else:
							if algorithm == "best":
								startPos = findBestFitLocation(frameSize)
								if startPos == -1:
									newKey = defrag(frameSize, key, pid, startTimeCounter, endTimeCounter)
									startPos = findBestFitLocation(frameSize)
									if startPos != -1:
										key = newKey

						if startPos == -1:
							print ("time {}ms: Cannot place process {} -- skipped!".format(key, pid))
							currentTime = key
						else:
							if algorithm != "nonContig":
								print ("time {}ms: Placed process {}:".format(key, pid))
								currentTime = key
								for pos in range(startPos, startPos+frameSize):
									memory[pos] = pid
								# print memory
								printMemory()
					break
				val+=1
			startTimeCounter+=1
			continue

def parseFile(fileName):
	global initialStartTime, initialEndTime
	openFile = open(fileName,'r')

	for line in openFile:
		splitLine = line.split()
		if len(splitLine) == 0:
			continue

		if len(splitLine) < 2:
			print ("ERROR: Not enough arguments in line")
			exit()

		if splitLine[0].isalpha() and len(splitLine[0]) == 1:
			PID = splitLine[0]
		elif '#' in splitLine[0]:
			continue
		else:
			print ("ERROR: Not enough arguments")
			exit()

		try:
			processSize = int(splitLine[1])
		except ValueError:
			print ("ERROR: Not enough arguments")
			exit()

		# processSize = splitLine[1]
		processes[PID] = int(processSize)

		for x in range(2, len(splitLine)):
			# print splitLine[x]
			splitTime = splitLine[x].split("/")
			# print( splitTime[0] , splitTime[1])
			# start = int(splitTime[0])
			try:
				start = int(splitTime[0])
			except ValueError:
				print ("ERROR: Not enough arguments")
				exit()

			# end = int(splitTime[0]) + int(splitTime[1])
			try:
				end = int(splitTime[0]) + int(splitTime[1])
			except ValueError:
				print ("ERROR: Not enough arguments")
				exit()

			if start in startTimes:
				startTimes[start].append(PID)
				startTimes[start].sort()
			else:
				startTimes[start] = [PID]

			if end in endTimes:
				endTimes[end].append(PID)
				endTimes[end].sort()
			else:
				endTimes[end] = [PID]
	initialStartTime = startTimes
	initialEndTime = endTimes

def reinitalizeGlobalVars():
	global memory, startTimes, endTimes, currentPos, currentTime, initialStartTime, initialEndTime
	# processes = {}
	memory = []
	for i in range(0,numFrames):
		memory.append('.')
	startTimes = initialStartTime
	endTimes = initialEndTime
	currentPos = 0
	currentTime = 0


if __name__ == '__main__':
	# if(not(sys.argv[1] or sys.argv[2] or sys.argv[3] or sys.argv[4])):
	# 	print ("ERROR: Not enough arguments")
	# 	exit()
	# print (sys.argv)
	# inputStuff = sys.argv
	if len(sys.argv) != 5:
		print ("ERROR: Not enough arguments")
		exit()

	try:
		framePerLine = int(sys.argv[1])
	except ValueError:
		print ("ERROR: Not enough arguments")
		exit()
	# framePerLine = int(sys.argv[1])
	try:
		numFrames = int(sys.argv[2])
	except ValueError:
		print ("ERROR: Not enough arguments")
		exit()
	# numFrames = int(sys.argv[2])
	fileName = sys.argv[3]
	try:
		defragTimePerMove = int(sys.argv[4])
	except ValueError:
		print ("ERROR: Not enough arguments")
		exit()
	# defragTimePerMove = sys.argv[4]


	parseFile(fileName)

	for i in range(0,numFrames):
		memory.append('.')

	print ("time {}ms: Simulator started (Contiguous -- First-Fit)".format(currentTime))
	runSim("first")
	print ("time {}ms: Simulator ended (Contiguous -- First-Fit)\n".format(currentTime))	

	reinitalizeGlobalVars()

	print ("time {}ms: Simulator started (Contiguous -- Next-Fit)".format(currentTime))
	runSim("next")
	print ("time {}ms: Simulator ended (Contiguous -- Next-Fit)\n".format(currentTime))

	reinitalizeGlobalVars()

	print ("time {}ms: Simulator started (Contiguous -- Best-Fit)".format(currentTime))
	runSim("best")
	print ("time {}ms: Simulator ended (Contiguous -- Best-Fit)\n".format(currentTime))

	reinitalizeGlobalVars()

	print ("time {}ms: Simulator started (Non-Contiguous)".format(currentTime))
	runSim("nonContig")
	print ("time {}ms: Simulator ended (Non-Contiguous)".format(currentTime), end='')


	