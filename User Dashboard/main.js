document.addEventListener("DOMContentLoaded", function () {
    const CONFIG = {
        channelIDs: ['2409417'],                            // Add all your channel IDs here
        apiKey: '579VT12O9GUIU6IA',
        updateInterval: 2000,
    };

    const FIELDS = [
        { name: 'field1', displayName: 'Temperature', link: getChartLink(1) },
        { name: 'field2', displayName: 'Dia', link: getChartLink(2) },
        { name: 'field3', displayName: 'Sys', link: getChartLink(3) },
        { name: 'field4', displayName: 'SpO2', link: getChartLink(4) },
        { name: 'field5', displayName: 'HR', link: getChartLink(5) },
        { name: 'field6', displayName: 'Field 6', link: getChartLink(6) },
        { name: 'field7', displayName: 'Field 7', link: getChartLink(7) },
        { name: 'field8', displayName: 'Fall Detected', link: getChartLink(8) }
    ];

    function getChartLink(channelID, fieldNumber) {
        return `https://thingspeak.com/channels/${channelID}/charts/${fieldNumber}?bgcolor=%23ffffff&color=%23d62020&dynamic=true&results=60&type=line&update=15`;
    }

    function updateTable(data) {
        const tableBody = document.querySelector('#dataTable tbody');
        tableBody.innerHTML = '';

        data.forEach((nodeData, index) => {
            if (nodeData.feeds && nodeData.feeds.length > 0) {
                const latestEntry = nodeData.feeds[0];
                const valuesRow = document.createElement('tr');
                
                // Add Node ID
                const nodeCell = document.createElement('td');
                nodeCell.textContent = `${nodeData.channel.id}`;
                valuesRow.appendChild(nodeCell);

                FIELDS.forEach(field => {
                    const fieldValue = latestEntry[field.name] || '-';
                    const cell = document.createElement('td');
                    cell.textContent = fieldValue;
                    if (field.name !== 'field8') {
                        cell.addEventListener('click', () => updateGraph(field.name, nodeData.channel.id));
                    }
                    valuesRow.appendChild(cell);
                });

                // Check if fall is detected
                if (latestEntry.field8 && (latestEntry.field8 === '1' || latestEntry.field8.toLowerCase() === 'true')) {
                    valuesRow.classList.add('fall-detected');
                }
                
                tableBody.appendChild(valuesRow);
            }
        });

        if (tableBody.children.length === 0) {
            console.error('No data or empty feeds for all nodes.');
        }
    }

    function updateGraph(fieldName, channelID) {
        const graphContainer = document.getElementById('graphContainer');
        const graphFrame = document.getElementById('graphFrame');
        const graphTitle = document.getElementById('graphTitle');
        const selectedField = FIELDS.find(f => f.name === fieldName);

        if (selectedField) {
            graphFrame.src = selectedField.link(channelID, FIELDS.indexOf(selectedField) + 1);
            graphTitle.textContent = `${selectedField.displayName} Graph - Node ${CONFIG.channelIDs.indexOf(channelID) + 1}`;
            graphContainer.style.display = 'block';
            
            // Trigger reflow to ensure the fade-in animation works
            void graphFrame.offsetWidth;
            
            graphFrame.classList.add('fade-in');
        }
    }

    function closeGraph() {
        const graphContainer = document.getElementById('graphContainer');
        const graphFrame = document.getElementById('graphFrame');
        
        graphFrame.classList.remove('fade-in');
        graphContainer.style.display = 'none';
    }
    
    function fetchData() {
        Promise.all(CONFIG.channelIDs.map(channelID => 
            fetch(`https://api.thingspeak.com/channels/${channelID}/feeds.json?api_key=${CONFIG.apiKey}&results=1`)
                .then(response => {
                    if (!response.ok) {
                        throw new Error('Network response was not ok');
                    }
                    return response.json();
                })
        ))
        .then(updateTable)
        .catch(error => console.error('Error fetching or updating data:', error.message));
    }

    // Initial data fetch
    fetchData();

    // Set up interval for data updates
    setInterval(fetchData, CONFIG.updateInterval);

    // Set up event listener for closing the graph
    document.getElementById('closeGraphBtn').addEventListener('click', closeGraph);
});
