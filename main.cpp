#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "json.hpp"

using json = nlohmann::json;
using namespace std;

class Game
{
private:
    json mapData; // JSON map data from map file
    string currentRoom; // Room player is currently in
    vector<string> playerInventory; // Inventory to store picked objects
    int playerLives; // Amount of lives

    // Method used when entering a new room (or loading into initial room)
    void enterRoom(const string &roomID)
    {
        auto it = find_if(mapData["rooms"].begin(), mapData["rooms"].end(),
                          [&roomID](const json &room)
                          { return room["id"] == roomID; });

        if (it == mapData["rooms"].end())
        {
            cout << "Room " << roomID << " does not exist." << endl;
            return;
        }

        const json &room = *it;

        if (room.contains("desc") && !room["desc"].is_null())
        {
            cout << "You are in: " << room["desc"] << endl;
        }
        else
        {
            cout << "This room has no description." << endl;
        }

        // Check for objects in the room (not picked by the player)
        for (auto &object : mapData["objects"])
        {
            if (object["initialroom"] == roomID &&
                find(playerInventory.begin(), playerInventory.end(), object["id"]) == playerInventory.end())
            {
                cout << "Object: " << object["id"] << " - " << object["desc"] << endl;
            }
        }

        // Check for enemies in room
        for (auto &enemy : mapData["enemies"])
        {
            if (enemy["initialroom"] == roomID)
            {
                cout << "Enemy: " << enemy["id"] << " - " << enemy["desc"] << endl;
            }
        }

        // Check for completing 'arrive at a certain room' objective
        if (mapData["objective"]["type"] == "room")
        {
            // Handling the case where the objective might be an array
            if (mapData["objective"]["what"].is_array())
            {
                auto &targetRooms = mapData["objective"]["what"];
                if (find(targetRooms.begin(), targetRooms.end(), roomID) != targetRooms.end())
                {
                    cout << "Congratulations! You have reached the target room and completed the objective." << endl;
                    exit(0); // Terminate program after game winner
                }
            }
            else if (mapData["objective"]["what"].is_string())
            {
                // If the objective is a single string
                if (mapData["objective"]["what"] == roomID)
                {
                    cout << "Congratulations! You have reached the target room and completed the objective." << endl;
                    exit(0); // Terminate the program after game winner
                }
            }
        }
    }

    // Method for picking up objects
    void pickUpObject(const string &objectName)
    {
        auto objectIt = find_if(mapData["objects"].begin(), mapData["objects"].end(),
                                [this, &objectName](const json &object)
                                {
                                    return object["id"] == objectName && object["initialroom"] == currentRoom;
                                });

        if (objectIt != mapData["objects"].end())
        {
            // Add object to the player's inventory
            playerInventory.push_back(objectName);
            cout << "Picked up: " << objectName << endl;

            // Remove object from the room
            mapData["objects"].erase(objectIt);

            // Check if objective is completed
            if (mapData["objective"]["type"] == "collect")
            {
                vector<string> requiredObjects = mapData["objective"]["what"].get<vector<string>>();
                if (all_of(requiredObjects.begin(), requiredObjects.end(),
                           [this](const string &obj)
                           { return find(playerInventory.begin(), playerInventory.end(), obj) != playerInventory.end(); }))
                {
                    cout << "Congratulations! You have completed the objective by collecting all required objects." << endl;
                    exit(0); // Terminate program after game winner
                }
            }
        }
        else
        {
            cout << "No such object to pick up in this room." << endl;
        }
    }

    // Method used when listing all items in player inventory
    void listItems()
    {
        if (playerInventory.empty()) // Check if empty
        {
            cout << "You are not carrying any items." << endl;
            return;
        }

        cout << "Items in your inventory:" << endl; // Lists items if not emtpy
        for (const auto &item : playerInventory)
        {
            cout << "- " << item << endl;
        }
    }

    // Method used when looking at items (In inventory or in current room)
    void lookAtObject(const string &objectName)
    {
        // Check player's inventory first
        if (find(playerInventory.begin(), playerInventory.end(), objectName) != playerInventory.end())
        {
            const auto &object = *find_if(mapData["objects"].begin(), mapData["objects"].end(),
                                          [&objectName](const json &obj)
                                          { return obj["id"] == objectName; });
            cout << objectName << ": " << object["desc"] << endl;
            return;
        }

        // Check if the object is in the current room
        auto it = find_if(mapData["objects"].begin(), mapData["objects"].end(),
                          [this, &objectName](const json &object)
                          {
                              return object["id"] == objectName && object["initialroom"] == currentRoom;
                          });

        if (it != mapData["objects"].end())
        {
            cout << objectName << ": " << it->at("desc") << endl;
        }
        else
        {
            cout << "No such object here or in your inventory." << endl;
        }
    }

    // Method used when eating apples for health
    void eatObject(const string &objectName)
    {
        auto it = find_if(playerInventory.begin(), playerInventory.end(),
                          [&objectName](const string &item)
                          { return item == objectName; });

        if (it != playerInventory.end())
        {
            // Remove the apple from the inventory
            playerInventory.erase(it);

            // Update the player's status or perform any other actions
            cout << "You have eaten the " << objectName << " and gained one life!" << endl;
            playerLives++;
        }
        else
        {
            cout << "Unable to eat this item. Please ensure the item is edible and in your inventory." << endl;
        }
    }

    int getPlayerLives() const
    {
        return playerLives;
    }

    void checkHealth() const
    {
        cout << "Your current lives: " << getPlayerLives() << " lives." << endl;
    }

public:
    // Constructor of game class
    Game(const string &jsonFile)
        : playerLives(3)    // Sets lives
    {
        ifstream file(jsonFile); // Map data is gotten from map file (JSON)
        if (!file.is_open())
        {
            throw runtime_error("Error opening file: " + jsonFile);
        }
        file >> mapData;
        file.close();

        if (mapData.contains("player") && mapData["player"].contains("initialroom") && !mapData["player"]["initialroom"].is_null())
        {
            currentRoom = mapData["player"]["initialroom"];
            enterRoom(currentRoom);
        }
        else
        {
            throw runtime_error("Player's initial room is not defined or invalid in the JSON file."); // Error is thrown if correct data is not availible
        }
    }

    // Method used when killing enemy
    void killEnemy(const string &enemyName)
    {
        auto it = find_if(mapData["enemies"].begin(), mapData["enemies"].end(),
                          [this, &enemyName](const json &enemy)
                          { return enemy["id"] == enemyName && enemy["initialroom"] == currentRoom; });
        if (it != mapData["enemies"].end())
        {
            const json &enemy = *it;
            const vector<string> &requiredObjects = enemy["killedby"];

            if (all_of(requiredObjects.begin(), requiredObjects.end(),
                       [this](const string &obj)
                       { return find(playerInventory.begin(), playerInventory.end(), obj) != playerInventory.end(); }))
            {
                cout << "You have successfully killed the " << enemyName << "." << endl;

                // Remove enemy from game
                mapData["enemies"].erase(it);

                // Check if objective is completed
                if (mapData["objective"]["type"] == "kill")
                {
                    vector<string> remainingEnemies = mapData["objective"]["what"].get<vector<string>>();
                    remainingEnemies.erase(remove(remainingEnemies.begin(), remainingEnemies.end(), enemyName), remainingEnemies.end());

                    mapData["objective"]["what"] = remainingEnemies;

                    if (remainingEnemies.empty())
                    {
                        cout << "Congratulations! You have completed the objective by killing all required enemies." << endl;
                        exit(0); // Terminate program after game winner
                    }
                }
            }
            else
            {
                cout << "You don't have the right set of objects to kill the " << enemyName << "." << endl;
                playerDies();
            }
        }
        else
        {
            cout << "No such enemy in this room." << endl;
        }
    }

    // Method for when player is killed
    void playerDies()
    {
        if (playerLives > 1)
        {
            playerLives--;
            cout << "You have " << playerLives << " lives remaining." << endl;
        }
        else
        {
            cout << "Game over! The enemy has killed you. You have run out of lives." << endl;
            exit(0); // Terminate program after game over message
        }
    }

    // Lists all availible commands to the user
    void listCommands() const {
        cout << "Available commands:" << endl;
        cout << "- go [direction]: Move to another room in the specified direction." << endl;
        cout << "- take [object]: Pick up an object from the current room." << endl;
        cout << "- list items: Display items in your inventory." << endl;
        cout << "- look or look around: Get a description of the current room and visible items." << endl;
        cout << "- look [object]: Examine a specific object." << endl;
        cout << "- eat [object]: Eat an edible object from your inventory." << endl;
        cout << "- check lives: Check your current number of lives." << endl;
        cout << "- kill [enemy]: Attempt to kill an enemy in the current room." << endl;
        cout << "- ?: List all available commands." << endl;
        cout << "- exit: Exit the game." << endl;
    }

    // This method is used to process commands including go, take, list items, look, eat, check lives and list all commands.
    void processCommand(const string &command)
    {
        if (command == "?") {   // List all commands
            listCommands();
        } else {
            if (command == "look" || command == "look around")  // Look comamnd
            {
                enterRoom(currentRoom);
            }
            else if (command.rfind("go ", 0) == 0)
            {
                string direction = command.substr(3);
                auto it = find_if(mapData["rooms"].begin(), mapData["rooms"].end(),
                                [this](const json &room)
                                { return room["id"] == currentRoom; });

                if (it != mapData["rooms"].end() && it->contains("exits") && it->at("exits").contains(direction))
                {
                    // Check if there is an enemy in the current room before leaving
                    auto enemyIt = find_if(mapData["enemies"].begin(), mapData["enemies"].end(),
                                        [this](const json &enemy)
                                        { return enemy["initialroom"] == currentRoom; });

                    if (enemyIt != mapData["enemies"].end())
                    {
                        const json &enemy = *enemyIt;
                        int aggressiveness = enemy["aggressiveness"];

                        // Determine whether the enemy attacks based on aggressiveness
                        int randint = (rand() % 100 + 1);
                        // cout << randint << endl; // Uncomment to test aggressiveness logic
                        if (randint < aggressiveness)
                        {
                            // Enemy attacks and player dies
                            cout << "The enemy attacks you as you try to leave you lose a life!" << endl;
                            playerDies();
                        }
                        else
                        {
                            // Enemy does not attack, allow the player to leave
                            cout << "You successfully leave the room with an enemy." << endl;
                        }
                    }
                    else
                    {
                        // No enemy in the current room, allow the player to leave
                        cout << "You successfully leave the room, there was no enemy." << endl;
                    }

                    // Update the current room after leaving
                    currentRoom = it->at("exits")[direction];
                    enterRoom(currentRoom);
                }
                else
                {
                    cout << "You can't go that way." << endl;
                }
            }
            else if (command.rfind("take ", 0) == 0) // Take command
            {
                string objectName = command.substr(5);
                pickUpObject(objectName);
            }
            else if (command == "list items") // List items command
            {
                listItems();
            }
            else if (command.rfind("look ", 0) == 0) // Look command
            {
                string objectName = command.substr(5);
                lookAtObject(objectName);
            }
            else if (command.rfind("eat", 0) == 0) // Eat command
            {
            if (command.size() > 4) // Ensure the command string is long enough
            {
                string objectName = command.substr(4);
                eatObject(objectName);
            }
            else
            {
                cout << "Please specify what you want to eat." << endl;
            }
        }
            else if (command == "check lives") // Check lives command
            {
                checkHealth();
            }
            else
            {
                cout << "I don't understand that command." << endl; // Invalid command
            }
        }
    }
};

// Main
int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Usage: ./main [json_file]" << endl;
        return 1;
    }

    try // Try block
    {
        Game game(argv[1]);
        string command;
        while (true)
        {
            cout << "> ";
            getline(cin, command);
            if (command == "exit")
                break;

            if (command.rfind("kill ", 0) == 0)
            {
                string enemyName = command.substr(5);
                try
                {
                    game.killEnemy(enemyName);
                }
                catch (const exception &e)
                {
                    cerr << "Error during kill command: " << e.what() << endl;
                }
            }
            else
            {
                game.processCommand(command);
            }
        }
    }
    catch (const exception &e) // Catch exception block
    {
        cerr << e.what() << endl;
        return 1;
    }

    return 0;
}
