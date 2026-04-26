#ifndef PAGE_NAVIGATION_H
#define PAGE_NAVIGATION_H

#include <Arduino.h>

class PageNavigation {
  private:
    int _rowsPerPage = 6; // Default 6 baris
    int _totalItems = 0;

    // Function untuk update page dan firstRowIndex berdasarkan selectedData semasa
    void updateState() {
      if (_totalItems == 0) {
        tablePage = 1;
        totalPage = 1;
        firstRowIndex = 0;
        selectedData = 0;
        return;
      }

      // Kira jumlah page
      // Formula: (total + rows - 1) / rows adalah cara integer math untuk 'ceiling'
      totalPage = (_totalItems + _rowsPerPage - 1) / _rowsPerPage;
      if (totalPage < 1) totalPage = 1;

      // Pastikan selectedData dalam range yang betul
      if (selectedData >= _totalItems) selectedData = _totalItems - 1;
      if (selectedData < 0) selectedData = 0;

      // Tentukan page semasa berdasarkan cursor (selectedData)
      tablePage = (selectedData / _rowsPerPage) + 1;

      // Tentukan index pertama untuk page tersebut
      firstRowIndex = (tablePage - 1) * _rowsPerPage;
    }

  public:
    int selectedData = 0;   // Index data yang dipilih (0, 1, 2...)
    int tablePage = 1;      // Nombor page semasa (1, 2...)
    int totalPage = 1;      // Jumlah page
    int firstRowIndex = 0;  // Index data pertama dalam page semasa

    // Function untuk reset cursor dan page kembali ke awal
    void reset() {
      selectedData = 0;
      updateState();
    }

    // Constructor kosong
    PageNavigation() {}

    // Tetapkan berapa baris dalam satu page
    void setRowInOnePage(int row) {
      _rowsPerPage = row;
    }

    // Tetapkan jumlah data (update setiap kali data berubah)
    void setTotalIndex(int totalIndex) {
      _totalItems = totalIndex;
      updateState(); // Recalculate everything
    }

    // --- NAVIGATION FUNCTIONS ---

    // Logic: Gerak bawah, kalau hujung page -> next page. Kalau last item -> balik index 0
    void goDown() {
      if (_totalItems == 0) return;
      selectedData++;
      
      if (selectedData >= _totalItems) {
        selectedData = 0; // Wrap ke awal
      }
      updateState();
    }

    // Logic: Gerak atas, kalau hujung page -> prev page. Kalau index 0 -> pergi last item
    void goUp() {
      if (_totalItems == 0) return;
      selectedData--;

      if (selectedData < 0) {
        selectedData = _totalItems - 1; // Wrap ke akhir
      }
      updateState();
    }

    // Logic: Lompat terus ke page seterusnya (kekalkan posisi relative cursor atau reset ke atas page)
    // Di sini saya buat ia pergi ke item pertama page seterusnya
    void nextPage() {
      if (totalPage <= 1) return;
      
      int jumpIndex = firstRowIndex + _rowsPerPage;
      if (jumpIndex >= _totalItems) jumpIndex = 0; // Balik page 1
      
      selectedData = jumpIndex;
      updateState();
    }

    // Logic: Lompat ke page sebelumnya
    void prevPage() {
      if (totalPage <= 1) return;

      int jumpIndex = firstRowIndex - _rowsPerPage;
      if (jumpIndex < 0) {
        // Pergi ke page terakhir (item pertama page terakhir)
        jumpIndex = (totalPage - 1) * _rowsPerPage;
      }

      selectedData = jumpIndex;
      updateState();
    }
};

#endif